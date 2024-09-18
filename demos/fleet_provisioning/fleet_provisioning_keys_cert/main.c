#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* corePKCS11 includes. */
#include "core_pkcs11.h"
#include "core_pkcs11_config.h"
#include "pkcs11_operations.h"

#include "mbedtls_pkcs11_posix.h"
#include "pal_queue.h"

#include "demo_config.h"

#define DEVICE_SERIAL_NUMBER_MAX    32

static struct NetworkContext networkContext = { 0 };
static char mqttEndpoint[] = AWS_IOT_ENDPOINT;
static char provisioningTemplateName[] = PROVISIONING_TEMPLATE_NAME;

extern bool pkcs11CloseSession( CK_SESSION_HANDLE p11Session );
extern int ProvisionDevicePKCS11WithFP( NetworkContext_t * pNetworkContext,
                                        CK_SESSION_HANDLE p11Session,
                                        char * pDeviceSerialNumber,
                                        char * pMqttEndpoint,
                                        char * pProvisioningTemplateName );

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

static int applicationLoop( void )
{
}

static void disconnectMQTT( void )
{
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

iotshdPal_SyncQueue_t * pSyncQueue = NULL;

void * prvProducerThread( void * arg )
{
    int i = 0;
    printf( "producerThread \r\n" );
    while( true )
    {
        iotshdPal_syncQueueSend( pSyncQueue, &i, 100 );
        printf( "Send %d\r\n", i );
        i = i + 1;
        sleep( 1 );
    }
    pthread_exit(NULL);
}

void * prvConsumerThread( void * arg )
{
    int i;
    bool retStatus;

    printf( "consumerThread \r\n" );
    while( true )
    {
        retStatus = iotshdPal_syncQueueReceive( pSyncQueue, &i, 100 );
        if( retStatus == true ) printf( "Recv %d\r\n", i );
    }
    pthread_exit(NULL);
}

static void prvPalQueueTest( void )
{
    pthread_t producerThread, producerThread1, consumerThread;

    printf( "prvPalQueueTest \r\n" );

    pSyncQueue = iotshdPal_syncQueueCreate( 10, sizeof( int ) );

    /* Create producer thread. */
    pthread_create( &producerThread, NULL, prvProducerThread, 0 );
    pthread_create( &producerThread1, NULL, prvProducerThread, 0 );

    /* Create consumer thread. */
    pthread_create( &consumerThread, NULL, prvConsumerThread, 0 );

    pthread_join( producerThread, NULL);
    
    pthread_join( consumerThread, NULL);
    
    printf( "prvPalQueueTest done\r\n" );

    while( 1 );
}

void main( void )
{
#if 1
    prvPalQueueTest();
#else
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
        retApplication = applicationLoop();

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
#endif
}
