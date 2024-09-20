#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* corePKCS11 includes. */
#include "core_pkcs11.h"
#include "core_pkcs11_config.h"
#include "pkcs11_operations.h"

#include "mbedtls_pkcs11_posix.h"

#include "mqtt_agent.h"

#include "demo_config.h"

/*-----------------------------------------------------------*/

#define DEVICE_SERIAL_NUMBER_MAX    32

/*-----------------------------------------------------------*/

static struct NetworkContext networkContext = { 0 };
static char mqttEndpoint[] = AWS_IOT_ENDPOINT;
static char provisioningTemplateName[] = PROVISIONING_TEMPLATE_NAME;

extern bool pkcs11CloseSession( CK_SESSION_HANDLE p11Session );
extern int ProvisionDevicePKCS11WithFP( NetworkContext_t * pNetworkContext,
                                        CK_SESSION_HANDLE p11Session,
                                        char * pDeviceSerialNumber,
                                        char * pMqttEndpoint,
                                        char * pProvisioningTemplateName );

/*-----------------------------------------------------------*/

/**
 * @brief Thread handle data structure definition.
 */
typedef void * FRTestThreadHandle_t;

/**
 * @brief Thread function to be executed in ThreadCreate_t function.
 *
 * @param[in] pParam The pParam parameter passed in ThreadCreate_t function.
 */
typedef void ( * FRTestThreadFunction_t )( void * pParam );

FRTestThreadHandle_t FRTest_ThreadCreate( FRTestThreadFunction_t threadFunc, void * pParam )
{
    FRTestThreadHandle_t threadHandle = NULL;

    int err = 0;
    pthread_t * pThreadid = NULL;
    pThreadid = ( FRTestThreadHandle_t ) malloc( sizeof( pthread_t ) );
    threadHandle = pThreadid;

    err = pthread_create( pThreadid, NULL, threadFunc, pParam );
    if( err != 0 )
    {
        return NULL;
    }
    return threadHandle;
}

/*-----------------------------------------------------------*/

static void devicePlatformInitialize( void )
{
}

static void deviceGetSerialNumber( char * pDeviceSerialNumber )
{
    strncpy( pDeviceSerialNumber, "TEST_SERIAL_NUMBER", DEVICE_SERIAL_NUMBER_MAX );
}

static void deviceWifiProvisioning( void )
{
}

static void deviceWIFIConnect( void )
{
}

static void establisihMQTTConnect( void )
{
}

static bool messageReceiveFlag = false;
static void prvDataModelHandlerIncommingPublish( void * pvIncomingPublishCallbackContext,
                                                 MQTTPublishInfo_t * pxPublishInfo )
{
    messageReceiveFlag = true;
}

void prvDataModelHandlerThread( void * pParam )
{
    MQTTPublishInfo_t xPublishInfo = { 0 };
    iotshdDev_MQTTAgentQueueItem_t * pQueueItem;

    iotshdDev_MQTTAgentUserContext_t * pUserContext;

    xPublishInfo.qos = 1;
    xPublishInfo.pTopicName = "test";
    xPublishInfo.topicNameLength = 4;
    xPublishInfo.pPayload = "HelloWorld\r\n";
    xPublishInfo.payloadLength = 12;

    pUserContext = iotshdDev_MQTTAgentCreateUserContext( 10 );


    while( 1 )
    {
        /* Subscribe the message topic. */
        iotshdDev_MQTTAgentAddSubscription( pUserContext,
                                            "test",
                                            4,
                                            prvDataModelHandlerIncommingPublish,
                                            NULL,
                                            5000 );
        /* Publish the message. */
        printf( "publish message\r\n" );
        messageReceiveFlag = false;
        iotshdDev_MQTTAgentPublish( pUserContext, &xPublishInfo, 100 );

        /* Waiting for the message received. */
        sleep( 3 );
        if( messageReceiveFlag == true )
        {
            printf( "Expected message is received\r\n" );
        }
        else
        {
            printf( "!!! Expected message is not received\r\n" );
        }

        /* Unsubscribe the message topic. */
        iotshdDev_MQTTAgentRemoveSubscription( pUserContext,
                                               "test",
                                               4,
                                               prvDataModelHandlerIncommingPublish,
                                               NULL,
                                               5000 );

        /* Publish the message. */
        printf( "publish message\r\n" );
        messageReceiveFlag = false;
        iotshdDev_MQTTAgentPublish( pUserContext, &xPublishInfo, 100 );

        /* Waiting for the message received. */
        sleep( 3 );
        if( messageReceiveFlag == true )
        {
            printf( "!!! Not expected message is received\r\n" );
        }
        else
        {
            printf( "Not expected message is not received\r\n" );
        }

        /* subscription with queue. */
        iotshdDev_MQTTAgentAddSubscriptionWithQueue( pUserContext,
                                                     "test",
                                                     4,
                                                     5000 );

        /* Publish the message. */
        printf( "publish message\r\n" );
        iotshdDev_MQTTAgentPublish( pUserContext, &xPublishInfo, 100 );
        iotshdDev_MQTTAgentPublish( pUserContext, &xPublishInfo, 100 );

        /* We should be able to receive from queue twice. */
        pQueueItem = iotshdDev_MQTTAgentDequeueIncommingPublish( pUserContext, 5000 );
        printf( "First queue item %s : %s\r\n", pQueueItem->publishInfo.pTopicName, pQueueItem->publishInfo.pPayload ); 
        iotshdDev_MQTTAgentFreeIncommingPublish( pUserContext, pQueueItem, false );

        pQueueItem = iotshdDev_MQTTAgentDequeueIncommingPublish( pUserContext, 5000 );
        printf( "Second queue item %s : %s\r\n", pQueueItem->publishInfo.pTopicName, pQueueItem->publishInfo.pPayload );
        iotshdDev_MQTTAgentFreeIncommingPublish( pUserContext, pQueueItem, false );

        /* subscription with queue. */
        iotshdDev_MQTTAgentRemoveSubscriptionWithQueue( pUserContext,
                                                        "test",
                                                        4,
                                                        5000 );

        /* We should not be able to receive from queue twice. */
        pQueueItem = iotshdDev_MQTTAgentDequeueIncommingPublish( pUserContext, 1000 );
        if( pQueueItem != NULL )
        {
            printf( "!!! receive unexpected incomming from queue\r\n" );
        }

        pQueueItem = iotshdDev_MQTTAgentDequeueIncommingPublish( pUserContext, 1000 );
        if( pQueueItem != NULL )
        {
            printf( "!!! receive unexpected incomming from queue\r\n" );
        }
    }
}

static int applicationLoop( CK_SESSION_HANDLE p11Session )
{
    MQTTStatus_t mqttStatus;
    FRTestThreadHandle_t dataModelHandlerThread;

    printf( "======================== application loop =================\r\n" );

    /* Initialize the MQTT agent. */
    mqttStatus = iotshdDev_MQTTAgentInit( &networkContext );

    /* Create another task for MQTT commands. */
    dataModelHandlerThread = FRTest_ThreadCreate( prvDataModelHandlerThread, NULL );

    /* Create another application task here. */
    mqttStatus = iotshdDev_MQTTAgentThreadLoop( &networkContext,
                                                mqttEndpoint,
                                                p11Session );

    return 0;
}

static void deviceInitializePKCS11Session( CK_SESSION_HANDLE * p11Session )
{
    CK_RV pkcs11ret = CKR_OK;
    pkcs11ret = xInitializePkcs11Session( p11Session );
}

static void deviceClosePKCS11Session( CK_SESSION_HANDLE p11Session )
{
    pkcs11CloseSession( p11Session );
}

void main( void )
{
    int retApplication;
    int retFPResult;
    char deviceSerialNumber[ DEVICE_SERIAL_NUMBER_MAX ];
    CK_SESSION_HANDLE p11Session;
    bool status;
    
    /* Device platform initialization code. */
    devicePlatformInitialize();

    /* Initialize the PKCS11 session for cryptographic operation. */
    deviceInitializePKCS11Session( &p11Session );

    /* Check device provisioned. */
    deviceWifiProvisioning();

    /* Insert the claim credentials into the PKCS #11 module */
    status = loadClaimCredentials( p11Session,
                                   CLAIM_CERT_PATH,
                                   pkcs11configLABEL_CLAIM_CERTIFICATE,
                                   CLAIM_PRIVATE_KEY_PATH,
                                   pkcs11configLABEL_CLAIM_PRIVATE_KEY );
    if( status == false )
    {
        LogError( ( "Failed to provision PKCS #11 with claim credentials." ) );
    }

    /* Network connection. */
    deviceWIFIConnect();

    /* Acquire the device serial number from device function. */
    deviceGetSerialNumber( deviceSerialNumber );


    for(;;)
    {
        retFPResult = ProvisionDevicePKCS11WithFP( &networkContext,
                                                   p11Session,
                                                   deviceSerialNumber,
                                                   mqttEndpoint,
                                                   provisioningTemplateName );
        if( 0 == retFPResult )
        {
            printf( "Successfully provision the device.\r\n" );
        }

        /* At this point, MQTT is connected. */
        retApplication = applicationLoop( p11Session );

        /*
        switch( retApplication )
        {
            case ERROR_MQTT_REMOTE_DISCONNECTED:
            case ERROR_TLS_CERTIFICATE_ERROR:
            case ERROR_TLS_CERTIFICATE_EXPIRED:
            case ERROR_MQTT_TIMEOUT:
            default:
                ...
        }
        */
        sleep( 5 );
    }

    deviceClosePKCS11Session( p11Session );
}
