#include "mqtt_agent.h"
#include "core_mqtt_agent.h"
#include "mbedtls_pkcs11_posix.h"
#include "core_pkcs11_config.h"
#include "core_pkcs11_config_defaults.h"

#include "demo_config.h"

/* MbedTLS transport include. */
#include "mbedtls_pkcs11_posix.h"

/*Include backoff algorithm header for retry logic.*/
#include "backoff_algorithm.h"

#include "subscription_manager.h"
#include "pal_queue.h"
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

#include "demo_config.h"

/* Clock for timer. */
#include "clock.h"

/*-----------------------------------------------------------*/

#define MQTT_AGENT_NETWORK_BUFFER_SIZE          4096
#define mqttexampleCONNACK_RECV_TIMEOUT_MS           ( 1000U )

#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 5U )
#define TRANSPORT_SEND_RECV_TIMEOUT_MS           ( 1000U )

#define mqttexampleMAX_COMMAND_SEND_BLOCK_TIME_MS   ( 1000U )

/**
 * @brief The length of the queue used to hold commands for the agent.
 */
#ifndef MQTT_AGENT_COMMAND_QUEUE_LENGTH
    #define MQTT_AGENT_COMMAND_QUEUE_LENGTH    ( 10U )
#endif

#define iotshdPal_Malloc    malloc
#define iotshdPal_Free      free

/*-----------------------------------------------------------*/

MQTTAgentContext_t xGlobalMqttAgentContext;
static uint8_t xNetworkBuffer[ MQTT_AGENT_NETWORK_BUFFER_SIZE ];

/**
 * @brief The parameters for MbedTLS operation.
 */
static MbedtlsPkcs11Context_t tlsContext = { 0 };

/**
 * @brief The global array of subscription elements.
 *
 * @note No thread safety is required to this array, since the updates the array
 * elements are done only from one task at a time. The subscription manager
 * implementation expects that the array of the subscription elements used for
 * storing subscriptions to be initialized to 0. As this is a global array, it
 * will be initialized to 0 by default.
 */
SubscriptionElement_t xGlobalSubscriptionList[ SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS ];

static MQTTAgentMessageContext_t xCommandQueue;

/*-----------------------------------------------------------*/

extern uint32_t Clock_GetTimeMs( void );

/*-----------------------------------------------------------*/

static uint32_t generateRandomNumber()
{
    return( ( uint32_t ) rand() );
}

/*-----------------------------------------------------------*/

static bool DisconnectMqttSession( NetworkContext_t * pNetworkContext )
{
    MQTTStatus_t mqttStatus = MQTTSuccess;
    bool returnStatus = false;
    MQTTContext_t * pMqttContext = &xGlobalMqttAgentContext.mqttContext;

    /* End TLS session, then close TCP connection. */
    ( void ) Mbedtls_Pkcs11_Disconnect( pNetworkContext );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static bool connectToBrokerWithBackoffRetries( NetworkContext_t * pNetworkContext,
                                               char * pMqttEndpoint,
                                               CK_SESSION_HANDLE p11Session,
                                               char * pClientCertLabel,
                                               char * pPrivateKeyLabel )
{
    bool returnStatus = false;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    MbedtlsPkcs11Status_t tlsStatus = MBEDTLS_PKCS11_SUCCESS;
    BackoffAlgorithmContext_t reconnectParams;
    MbedtlsPkcs11Credentials_t tlsCredentials = { 0 };
    uint16_t nextRetryBackOff = 0U;
    size_t xMqttEndpointLength;

    xMqttEndpointLength = strnlen( pMqttEndpoint, 128 );

    /* Set the pParams member of the network context with desired transport. */
    pNetworkContext->pParams = &tlsContext;

    /* Initialize credentials for establishing TLS session. */
    tlsCredentials.pRootCaPath = ROOT_CA_CERT_PATH;
    tlsCredentials.pClientCertLabel = pClientCertLabel;
    tlsCredentials.pPrivateKeyLabel = pPrivateKeyLabel;
    tlsCredentials.p11Session = p11Session;

    /* AWS IoT requires devices to send the Server Name Indication (SNI)
     * extension to the Transport Layer Security (TLS) protocol and provide
     * the complete endpoint address in the host_name field. Details about
     * SNI for AWS IoT can be found in the link below.
     * https://docs.aws.amazon.com/iot/latest/developerguide/transport-security.html
     */
    tlsCredentials.disableSni = false;

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams( &reconnectParams,
                                       CONNECTION_RETRY_BACKOFF_BASE_MS,
                                       CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                       CONNECTION_RETRY_MAX_ATTEMPTS );

    do
    {
        /* Establish a TLS session with the MQTT broker. This example connects
         * to the MQTT broker as specified in AWS_IOT_ENDPOINT and AWS_MQTT_PORT
         * at the demo config header. */
        LogInfo( ( "Establishing a TLS session to %.*s:%d.",
                    xMqttEndpointLength,
                    pMqttEndpoint,
                    AWS_MQTT_PORT ) );

        tlsStatus = Mbedtls_Pkcs11_Connect( pNetworkContext,
                                            pMqttEndpoint,
                                            AWS_MQTT_PORT,
                                            &tlsCredentials,
                                            TRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( tlsStatus == MBEDTLS_PKCS11_SUCCESS )
        {
            /* Connection successful. */
            printf( "TLS connected\r\n" );
            returnStatus = true;
        }
        else
        {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &reconnectParams, generateRandomNumber(), &nextRetryBackOff );

            if( backoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Connection to the broker failed, all attempts exhausted." ) );
            }
            else if( backoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the broker failed. Retrying connection "
                           "after %hu ms backoff.",
                           ( unsigned short ) nextRetryBackOff ) );
                Clock_SleepMs( nextRetryBackOff );
            }
        }
    } while( ( tlsStatus != MBEDTLS_PKCS11_SUCCESS ) && ( backoffAlgStatus == BackoffAlgorithmSuccess ) );

    return returnStatus;
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvMQTTConnect( bool xCleanSession )
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;

    /* Many fields are not used in this demo so start with everything at 0. */
    memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = xCleanSession;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = CLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( CLIENT_IDENTIFIER );

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet.  This responsibility will
     * be moved inside the agent. */
    xConnectInfo.keepAliveSeconds = 60;

    /* Append metrics string when connecting to AWS IoT Core with custom auth */
    xConnectInfo.pUserName = NULL;
    xConnectInfo.userNameLength = 0;
    xConnectInfo.pPassword = NULL;
    xConnectInfo.passwordLength = 0U;

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    xResult = MQTT_Connect( &( xGlobalMqttAgentContext.mqttContext ),
                            &xConnectInfo,
                            NULL,
                            mqttexampleCONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );

    LogInfo( ( "Session present: %d\n", xSessionPresent ) );

    /* Resume a session if desired. */
    if( ( xResult == MQTTSuccess ) && ( xCleanSession == false ) )
    {
        xResult = MQTTAgent_ResumeSession( &xGlobalMqttAgentContext, xSessionPresent );

        /* Resubscribe to all the subscribed topics. */
        if( ( xResult == MQTTSuccess ) && ( xSessionPresent == false ) )
        {
            // xResult = prvHandleResubscribe();
        }
    }

    return xResult;
}

/*-----------------------------------------------------------*/

static void prvIncomingPublishCallback( MQTTAgentContext_t * pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    bool xPublishHandled = false;
    char cOriginalChar, * pcLocation;

    ( void ) packetId;

    /* Fan out the incoming publishes to the callbacks registered using
     * subscription manager. */
    xPublishHandled = handleIncomingPublishes( ( SubscriptionElement_t * ) pMqttAgentContext->pIncomingCallbackContext,
                                               pxPublishInfo );

    /* If there are no callbacks to handle the incoming publishes,
     * handle it as an unsolicited publish. */
    if( xPublishHandled != true )
    {
        /* Ensure the topic string is terminated for printing.  This will over-
         * write the message ID, which is restored afterwards. */
        pcLocation = ( char * ) &( pxPublishInfo->pTopicName[ pxPublishInfo->topicNameLength ] );
        cOriginalChar = *pcLocation;
        *pcLocation = 0x00;
        LogWarn( ( "WARN:  Received an unsolicited publish from topic %s", pxPublishInfo->pTopicName ) );
        *pcLocation = cOriginalChar;
    }
}

/*-----------------------------------------------------------*/

MQTTStatus_t iotshdDev_MQTTAgentInit( NetworkContext_t * pxNetworkContext )
{
    TransportInterface_t xTransport;
    MQTTStatus_t xReturn;
    MQTTFixedBuffer_t xFixedBuffer = { .pBuffer = xNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE };

    MQTTAgentMessageInterface_t messageInterface =
    {
        .pMsgCtx        = NULL,
        .send           = Agent_MessageSend,
        .recv           = Agent_MessageReceive,
        .getCommand     = Agent_GetCommand,
        .releaseCommand = Agent_ReleaseCommand
    };

    xCommandQueue.queue = iotshdPal_syncQueueCreate( MQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                                     sizeof( MQTTAgentCommand_t * ) );
    messageInterface.pMsgCtx = &xCommandQueue;
    Agent_InitializePool();

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = Mbedtls_Pkcs11_Send;
    xTransport.recv = Mbedtls_Pkcs11_Recv;
    xTransport.writev = NULL;    

    /* Initialize MQTT library. */
    xReturn = MQTTAgent_Init( &xGlobalMqttAgentContext,
                              &messageInterface,
                              &xFixedBuffer,
                              &xTransport,
                              Clock_GetTimeMs,
                              prvIncomingPublishCallback,
                              xGlobalSubscriptionList );

    return xReturn;
}

/*-----------------------------------------------------------*/


MQTTStatus_t iotshdDev_MQTTAgentThreadLoop( NetworkContext_t * pNetworkContext,
                                            const char * pSmarthomeEndpoint,
                                            CK_SESSION_HANDLE * pxSession )
{
    bool xNetworkResult;
    MQTTStatus_t xMQTTStatus = MQTTSuccess, xConnectStatus = MQTTSuccess;
    MQTTContext_t * pMqttContext = &( xGlobalMqttAgentContext.mqttContext );

    ( void ) memset( pNetworkContext, 0U, sizeof( NetworkContext_t ) );

    xNetworkResult = connectToBrokerWithBackoffRetries( pNetworkContext,
                                                        pSmarthomeEndpoint,
                                                        pxSession,
                                                        pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS,
                                                        pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS );
    pMqttContext->connectStatus = MQTTNotConnected;

    /* MQTT Connect with a persistent session. */
    xConnectStatus = prvMQTTConnect( true );

    do
    {
        /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
         * will manage the MQTT protocol until such time that an error occurs,
         * which could be a disconnect.  If an error occurs the MQTT context on
         * which the error happened is returned so there can be an attempt to
         * clean up and reconnect however the application writer prefers. */
        xMQTTStatus = MQTTAgent_CommandLoop( &xGlobalMqttAgentContext );

        /* Success is returned for disconnect or termination. The socket should
         * be disconnected. */
        if( ( xMQTTStatus == MQTTSuccess ) && ( xGlobalMqttAgentContext.mqttContext.connectStatus == MQTTNotConnected ) )
        {
            /* MQTT Disconnect. Disconnect the socket. */
            ( void ) Mbedtls_Pkcs11_Disconnect( pNetworkContext );
        }
        else if( xMQTTStatus == MQTTSuccess )
        {
            /* MQTTAgent_Terminate() was called, but MQTT was not disconnected. */
            xMQTTStatus = MQTT_Disconnect( &( xGlobalMqttAgentContext.mqttContext ) );
            ( void ) Mbedtls_Pkcs11_Disconnect( pNetworkContext );
        }
        /* Error. */
        else
        {
            /* Reconnect TCP. */
            ( void ) Mbedtls_Pkcs11_Disconnect( pNetworkContext );
            xNetworkResult = connectToBrokerWithBackoffRetries( pNetworkContext,
                                                                pSmarthomeEndpoint,
                                                                pxSession,
                                                                pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS,
                                                                pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS );
            pMqttContext->connectStatus = MQTTNotConnected;

            /* MQTT Connect with a persistent session. */
            xConnectStatus = prvMQTTConnect( false );
        }
    } while( xMQTTStatus != MQTTSuccess );

    return MQTTSuccess;
}

MQTTStatus_t iotshdDev_MQTTAgentStop( void )
{
    return MQTTSuccess;
}


iotshdDev_MQTTAgentUserContext_t * iotshdDev_MQTTAgentCreateUserContext( uint32_t incommingPublishQueueSize )
{
    uint32_t i; 
    iotshdDev_MQTTAgentUserContext_t * pUserContext = NULL;
    iotshdDev_MQTTAgentQueueItem_t * pQueueItem;

    pUserContext = iotshdPal_Malloc( sizeof( iotshdDev_MQTTAgentUserContext_t ) );

    if( pUserContext != NULL )
    {
        memset( pUserContext, 0, sizeof( iotshdDev_MQTTAgentUserContext_t ) );

        pUserContext->pSyncEvent = iotshdPal_syncEventCreate();
        if( pUserContext->pSyncEvent != NULL )
        {
            pUserContext->queueSize = incommingPublishQueueSize;

            if( incommingPublishQueueSize > 0 )
            {
                pUserContext->pIncommingPublishQueue = iotshdPal_syncQueueCreate( incommingPublishQueueSize, sizeof( iotshdDev_MQTTAgentQueueItem_t * ) );
                pUserContext->pFreePublishMessageQueue = iotshdPal_syncQueueCreate( incommingPublishQueueSize, sizeof( iotshdDev_MQTTAgentQueueItem_t * ) );

                pUserContext->pQueueItems = iotshdPal_Malloc( incommingPublishQueueSize * sizeof( iotshdDev_MQTTAgentQueueItem_t ) );
                memset( pUserContext->pQueueItems, 0, incommingPublishQueueSize * sizeof( iotshdDev_MQTTAgentQueueItem_t ) );
                for( i = 0; i < incommingPublishQueueSize; i++ )
                {
                    pQueueItem = &pUserContext->pQueueItems[ i ];
                    iotshdPal_syncQueueSend( pUserContext->pFreePublishMessageQueue,
                                             &pQueueItem,
                                             0 );
                }
            }
        }
    }
    
    return pUserContext;
}

/**
 * @brief MQTT Agent delete user context.
 *
 * @param pUserContext user context to be deleted.
 */
void iotshdDev_MQTTAgentDeleteUserContext( iotshdDev_MQTTAgentUserContext_t * pUserContext )
{
    iotshdDev_MQTTAgentQueueItem_t * pQueueItem;
    if( pUserContext != NULL )
    {
        if( pUserContext->pSyncEvent != NULL )
        {
            iotshdPal_Free( pUserContext->pSyncEvent );
            pUserContext->pSyncEvent = NULL;
        }

        if( pUserContext->pFreePublishMessageQueue != NULL )
        {
            iotshdPal_syncQueueDelete( pUserContext->pFreePublishMessageQueue );
            while( iotshdPal_syncQueueReceive( pUserContext->pFreePublishMessageQueue, ( void * ) &pQueueItem, 0 ) == true )
            {
                iotshdPal_Free( pQueueItem->topicPayloadBuffer );
                pQueueItem->topicPayloadBuffer = NULL;
                pQueueItem->topicPayloadBufferSize = 0;
            }
            pUserContext->pFreePublishMessageQueue = NULL;
        }

        if( pUserContext->pIncommingPublishQueue != NULL )
        {
            iotshdPal_syncQueueDelete( pUserContext->pIncommingPublishQueue );
            while( iotshdPal_syncQueueReceive( pUserContext->pFreePublishMessageQueue, ( void * ) &pQueueItem, 0 ) == true )
            {
                iotshdPal_Free( pQueueItem->topicPayloadBuffer );
                pQueueItem->topicPayloadBuffer = NULL;
                pQueueItem->topicPayloadBufferSize = 0;
            }
            pUserContext->pIncommingPublishQueue = NULL;
        }

        if( pUserContext->pQueueItems != NULL )
        {
            iotshdPal_Free( pUserContext->pQueueItems );
            pUserContext->pQueueItems = NULL;
        }

        iotshdPal_Free( pUserContext );
    }
}

static void prvAgentPublishCommandCallback( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                           MQTTAgentReturnInfo_t * pxReturnInfo )
{
    /* Store the result in the application defined context so the task that
     * initiated the publish can check the operation's status. */
    pUserContext->xReturnStatus = pxReturnInfo->returnCode;

    if( pUserContext->pSyncEvent != NULL )
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        iotshdPal_syncEventSet( pUserContext->pSyncEvent );
    }
}

MQTTStatus_t iotshdDev_MQTTAgentPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                         MQTTPublishInfo_t * pPublishInfo,
                                         uint32_t blockTimeMs )
{
    MQTTStatus_t xCommandAdded;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    bool retStatus;

    xCommandParams.blockTimeMs = blockTimeMs;
    xCommandParams.cmdCompleteCallback = prvAgentPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = pUserContext;

    xCommandAdded = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                       pPublishInfo,
                                       &xCommandParams );

    /* Waiting for callback notification. */
    if( xCommandAdded == MQTTSuccess )
    {
        retStatus = iotshdPal_syncEventWait( pUserContext->pSyncEvent, 1000 );

        if( retStatus == true )
        {
            xCommandAdded = pUserContext->xReturnStatus;
        }
    }

    return xCommandAdded;
}

typedef struct iotshdDev_MQTTAgentSubscribeContext
{
    iotshdDev_MQTTAgentUserContext_t * pUserContext;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs;

    const char * pcTopicFilterString;
    uint16_t usTopicFilterLength;

    IncomingPubCallback_t pxIncomingPublishCallback;
    void * pvIncomingPublishCallbackContext;

} iotshdDev_MQTTAgentSubscribeContext_t;

static void prvAgetSubscribeCommandCallback( void * pxCommandContext,
                                             MQTTAgentReturnInfo_t * pxReturnInfo )
{
    bool xSubscriptionAdded = false;
    iotshdDev_MQTTAgentSubscribeContext_t * pSubscribeContext = ( iotshdDev_MQTTAgentSubscribeContext_t * )pxCommandContext;
    iotshdDev_MQTTAgentUserContext_t * pUserContext;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs;
    
    if( pSubscribeContext != NULL )
    {
        pUserContext = pSubscribeContext->pUserContext;
        pxSubscribeArgs = pSubscribeContext->pxSubscribeArgs;

        /* Store the result in the application defined context so the task that
         * initiated the subscribe can check the operation's status.  Also send the
         * status as the notification value.  These things are just done for
         * demonstration purposes. */
        pUserContext->xReturnStatus = pxReturnInfo->returnCode;

        /* Check if the subscribe operation is a success. Only one topic is
         * subscribed by this demo. */
        if( pxReturnInfo->returnCode == MQTTSuccess )
        {
            /* Add subscription so that incoming publishes are routed to the application
             * callback. */
            xSubscriptionAdded = addSubscription( xGlobalSubscriptionList,
                                                  pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                                  pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                                  pSubscribeContext->pxIncomingPublishCallback,
                                                  pSubscribeContext->pvIncomingPublishCallbackContext );

            if( xSubscriptionAdded == false )
            {
                LogError( ( "Failed to register an incoming publish callback for topic %.*s.",
                            pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                            pxSubscribeArgs->pSubscribeInfo->pTopicFilter ) );
            }
        }

        /* Notify the thread waiting for the response. */
        iotshdPal_syncEventSet( pUserContext->pSyncEvent );
    }
}

MQTTStatus_t iotshdDev_MQTTAgentAddSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                 const char * pcTopicFilterString,
                                                 uint16_t usTopicFilterLength,
                                                 IncomingPubCallback_t pxIncomingPublishCallback,
                                                 void * pvIncomingPublishCallbackContext,
                                                 uint32_t blockTimeMs )
{
    /* Subscribe to the topic first. */
    MQTTStatus_t xCommandAdded;
    MQTTAgentSubscribeArgs_t xSubscribeArgs;
    MQTTSubscribeInfo_t xSubscribeInfo;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    iotshdDev_MQTTAgentSubscribeContext_t xSubscribeContext = { 0 };
    bool retStatus;

    /* Complete the subscribe information.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.pTopicFilter = pcTopicFilterString;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilterString );
    xSubscribeInfo.qos = MQTTQoS1;
    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    xCommandParams.blockTimeMs = blockTimeMs;
    xCommandParams.cmdCompleteCallback = prvAgetSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xSubscribeContext;

    xSubscribeContext.pUserContext = pUserContext;
    xSubscribeContext.pxSubscribeArgs = &xSubscribeArgs;
    
    xSubscribeContext.pcTopicFilterString = pcTopicFilterString;
    xSubscribeContext.usTopicFilterLength = usTopicFilterLength;
    
    xSubscribeContext.pxIncomingPublishCallback = pxIncomingPublishCallback;
    xSubscribeContext.pvIncomingPublishCallbackContext = pvIncomingPublishCallbackContext;

    xCommandAdded = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                         &xSubscribeArgs,
                                         &xCommandParams );
    if( xCommandAdded == MQTTSuccess )
    {
        /* Waiting for the command complete. */
        printf( "command success\r\n" );
        ( void ) iotshdPal_syncEventWait( pUserContext->pSyncEvent, 0 );
        retStatus = iotshdPal_syncEventWait( pUserContext->pSyncEvent, blockTimeMs );
        if( retStatus != true )
        {
            /* TODO : handle if the callback is not called situation here.
             * the stack memory should not be used in the callback. */
        }
    }
    else
    {
        printf( "Add command fail\r\n" );
    }

    return xCommandAdded;
}

static void prvAgetUnsubscribeCommandCallback( void * pxCommandContext,
                                               MQTTAgentReturnInfo_t * pxReturnInfo )
{
    bool xUnsubscriptionAdded = false;
    iotshdDev_MQTTAgentSubscribeContext_t * pSubscribeContext = ( iotshdDev_MQTTAgentSubscribeContext_t * )pxCommandContext;
    iotshdDev_MQTTAgentUserContext_t * pUserContext;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs;
    
    if( pSubscribeContext != NULL )
    {
        pUserContext = pSubscribeContext->pUserContext;
        pxSubscribeArgs = pSubscribeContext->pxSubscribeArgs;

        /* Store the result in the application defined context so the task that
         * initiated the subscribe can check the operation's status.  Also send the
         * status as the notification value.  These things are just done for
         * demonstration purposes. */
        pUserContext->xReturnStatus = pxReturnInfo->returnCode;

        /* Check if the subscribe operation is a success. Only one topic is
         * subscribed by this demo. */
        if( pxReturnInfo->returnCode == MQTTSuccess )
        {
            /* Add subscription so that incoming publishes are routed to the application
             * callback. */
            removeSubscription( xGlobalSubscriptionList,
                                pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                pxSubscribeArgs->pSubscribeInfo->topicFilterLength );
        }

        /* Notify the thread waiting for the response. */
        iotshdPal_syncEventSet( pUserContext->pSyncEvent );
    }
}

MQTTStatus_t iotshdDev_MQTTAgentRemoveSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                    const char * pcTopicFilterString,
                                                    uint16_t usTopicFilterLength,
                                                    IncomingPubCallback_t pxIncomingPublishCallback,
                                                    void * pvIncomingPublishCallbackContext,
                                                    uint32_t blockTimeMs )
{
    /* Unubscribe to the topic first. */
    MQTTStatus_t xCommandAdded;
    MQTTAgentSubscribeArgs_t xSubscribeArgs;
    MQTTSubscribeInfo_t xSubscribeInfo;
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    iotshdDev_MQTTAgentSubscribeContext_t xSubscribeContext = { 0 };
    bool retStatus;

    /* Complete the subscribe information.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.pTopicFilter = pcTopicFilterString;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilterString );
    xSubscribeInfo.qos = MQTTQoS1;
    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    xCommandParams.blockTimeMs = blockTimeMs;
    xCommandParams.cmdCompleteCallback = prvAgetUnsubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xSubscribeContext;

    xSubscribeContext.pUserContext = pUserContext;
    xSubscribeContext.pxSubscribeArgs = &xSubscribeArgs;
    
    xSubscribeContext.pcTopicFilterString = pcTopicFilterString;
    xSubscribeContext.usTopicFilterLength = usTopicFilterLength;
    
    xSubscribeContext.pxIncomingPublishCallback = pxIncomingPublishCallback;
    xSubscribeContext.pvIncomingPublishCallbackContext = pvIncomingPublishCallbackContext;

    xCommandAdded = MQTTAgent_Unsubscribe( &xGlobalMqttAgentContext,
                                           &xSubscribeArgs,
                                           &xCommandParams );
    if( xCommandAdded == MQTTSuccess )
    {
        /* Waiting for the command complete. */
        ( void ) iotshdPal_syncEventWait( pUserContext->pSyncEvent, 0 );
        retStatus = iotshdPal_syncEventWait( pUserContext->pSyncEvent, blockTimeMs );
        if( retStatus != true )
        {
            /* TODO : handle if the callback is not called situation here.
             * the stack memory should not be used in the callback. */
        }
    }
    else
    {
        printf( "Add command fail\r\n" );
    }

    return xCommandAdded;
}

void mqttAgentEnqueuePublishCallback( void * pCallbackContext, MQTTPublishInfo_t * pPublsihInfo )
{
    iotshdDev_MQTTAgentUserContext_t * pUserContext = ( iotshdDev_MQTTAgentUserContext_t * ) pCallbackContext;
    iotshdDev_MQTTAgentQueueItem_t * pQueueItem;
    size_t requiredTopicPayloadSize = 0;
    bool retStatus;

    /* Get the free slot from free incoming publish queue. */
    retStatus = iotshdPal_syncQueueReceive( pUserContext->pFreePublishMessageQueue, &pQueueItem, 0 );
    
    if( retStatus == true )
    {
        /* Dup the publish info structure. */
        memcpy( &pQueueItem->publishInfo, pPublsihInfo, sizeof( MQTTPublishInfo_t ) );

        /* Calcualte required topic payload size. */
        requiredTopicPayloadSize = pPublsihInfo->topicNameLength + 1U + pPublsihInfo->payloadLength + 1U;

        if( pQueueItem->topicPayloadBuffer != NULL )
        {
            if( pQueueItem->topicPayloadBufferSize < requiredTopicPayloadSize )
            {
                /* The buffer size is not enough. Re-allocate a new buffer. */
                iotshdPal_Free( pQueueItem->topicPayloadBuffer );
                pQueueItem->topicPayloadBuffer = NULL;
                pQueueItem->topicPayloadBufferSize = 0;
            }
        }

        if( pQueueItem->topicPayloadBuffer == NULL )
        {
            pQueueItem->topicPayloadBuffer = iotshdPal_Malloc( requiredTopicPayloadSize );
            pQueueItem->topicPayloadBufferSize = requiredTopicPayloadSize;
        }

        memcpy( pQueueItem->topicPayloadBuffer, pPublsihInfo->pTopicName, pPublsihInfo->topicNameLength );
        pQueueItem->topicPayloadBuffer[ pPublsihInfo->topicNameLength ] = '\0';
        pQueueItem->publishInfo.pTopicName = ( char * )pQueueItem->topicPayloadBuffer;

        memcpy( &( pQueueItem->topicPayloadBuffer[ pPublsihInfo->topicNameLength + 1U ] ),
                pPublsihInfo->pPayload,
                pPublsihInfo->payloadLength );
        pQueueItem->topicPayloadBuffer[ pPublsihInfo->topicNameLength + 1U + pPublsihInfo->payloadLength ] = '\0';
        pQueueItem->publishInfo.pPayload = ( char * )( &pQueueItem->topicPayloadBuffer[ pPublsihInfo->topicNameLength + 1U ] );

        /* Enqueue the incomming publish. */
        iotshdPal_syncQueueSend( pUserContext->pIncommingPublishQueue, &pQueueItem, 0U );
    }
}

MQTTStatus_t iotshdDev_MQTTAgentAddSubscriptionWithQueue( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                  const char * pcTopicFilterString,
                                                  uint16_t usTopicFilterLength,
                                                  uint32_t blockTimeMs )
{
    return iotshdDev_MQTTAgentAddSubscription( pUserContext,
                                               pcTopicFilterString,
                                               usTopicFilterLength,
                                               mqttAgentEnqueuePublishCallback,
                                               pUserContext,
                                               blockTimeMs );
}

iotshdDev_MQTTAgentQueueItem_t * iotshdDev_MQTTAgentDequeueIncommingPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                                             uint32_t blockTimeMs )
{
    MQTTPublishInfo_t * pPublishInfo = NULL;
    iotshdDev_MQTTAgentQueueItem_t * pQueueItem = NULL;
    bool retStatus;
    retStatus = iotshdPal_syncQueueReceive( pUserContext->pIncommingPublishQueue, &pQueueItem, blockTimeMs );
    return pQueueItem;
}

void iotshdDev_MQTTAgentFreeIncommingPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                              iotshdDev_MQTTAgentQueueItem_t * pQueueItem,
                                              bool freeBuffer )
{
    if( freeBuffer == true )
    {
        if( pQueueItem->topicPayloadBuffer != NULL )
        {
            iotshdPal_Free( pQueueItem->topicPayloadBuffer );
            pQueueItem->topicPayloadBuffer = NULL;
            pQueueItem->topicPayloadBufferSize = 0;
        }
    }
    memset( &pQueueItem->publishInfo, 0, sizeof( MQTTPublishInfo_t ) );
    iotshdPal_syncQueueSend( pUserContext->pFreePublishMessageQueue, &pQueueItem, 0 );
}

MQTTStatus_t iotshdDev_MQTTAgentRemoveSubscriptionWithQueue( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                             const char * pcTopicFilterString,
                                                             uint16_t usTopicFilterLength,
                                                             uint32_t blockTimeMs )
{
    return iotshdDev_MQTTAgentRemoveSubscription( pUserContext,
                                                  pcTopicFilterString,
                                                  usTopicFilterLength,
                                                  mqttAgentEnqueuePublishCallback,
                                                  pUserContext,
                                                  blockTimeMs );
}
