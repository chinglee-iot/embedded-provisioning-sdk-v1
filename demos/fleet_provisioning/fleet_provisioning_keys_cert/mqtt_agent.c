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

/* Clock for timer. */
#include "clock.h"

/*-----------------------------------------------------------*/

#define MQTT_AGENT_NETWORK_BUFFER_SIZE          4096
#define mqttexampleCONNACK_RECV_TIMEOUT_MS           ( 1000U )

#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 5U )
#define TRANSPORT_SEND_RECV_TIMEOUT_MS           ( 1000U )

/*-----------------------------------------------------------*/

MQTTAgentContext_t xGlobalMqttAgentContext;
static uint8_t xNetworkBuffer[ MQTT_AGENT_NETWORK_BUFFER_SIZE ];

/**
 * @brief The parameters for MbedTLS operation.
 */
static MbedtlsPkcs11Context_t tlsContext = { 0 };

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
        LogDebug( ( "Establishing a TLS session to %.*s:%d.",
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

MQTTStatus_t iotshdDev_MQTTAgentInit( NetworkContext_t * pxNetworkContext )
{
    TransportInterface_t xTransport;
    MQTTStatus_t xReturn;
    MQTTFixedBuffer_t xFixedBuffer = { .pBuffer = xNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE };

    MQTTAgentMessageInterface_t messageInterface = { NULL };

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
                              NULL,
                              NULL );

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
    return NULL;
}

/**
 * @brief MQTT Agent delete user context.
 *
 * @param pUserContext user context to be deleted.
 */
void iotshdDev_MQTTAgentDeleteUserContext( iotshdDev_MQTTAgentUserContext_t * pUserContext )
{
}

MQTTStatus_t iotshdDev_MQTTAgentPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                         MQTTPublishInfo_t * pPublishInfo,
                                         uint32_t blockTimeMs )
{
    return MQTTSuccess;
}

MQTTStatus_t iotshdDev_MQTTAgentAddSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                 const char * pcTopicFilterString,
                                                 uint16_t usTopicFilterLength,
                                                 IncomingPubCallback_t pxIncomingPublishCallback,
                                                 void * pvIncomingPublishCallbackContext,
                                                 uint32_t blockTimeMs )
{
    return MQTTSuccess;
}

MQTTStatus_t iotshdDev_MQTTAgentRemoveSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                    const char * pcTopicFilterString,
                                                    uint16_t usTopicFilterLength,
                                                    IncomingPubCallback_t pxIncomingPublishCallback,
                                                    void * pvIncomingPublishCallbackContext,
                                                    uint32_t blockTimeMs )
{
    return MQTTSuccess;
}
