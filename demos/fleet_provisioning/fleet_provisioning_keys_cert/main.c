#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* corePKCS11 includes. */
#include "core_pkcs11.h"
#include "core_pkcs11_config.h"

#include "mbedtls_pkcs11_posix.h"

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

void main( void )
{
    int retApplication;
    int retFPResult;
    char deviceSerialNumber[ DEVICE_SERIAL_NUMBER_MAX ];
    CK_SESSION_HANDLE p11Session;
    
    /* Device platform initialization code. */
    devicePlatformInitialize();

    /* Check device provisioned. */
    deviceWifiProvisioning();

    /* Network connection. */
    deviceWIFIConnect();

    /* Acquire the device serial number from device function. */
    deviceGetSerialNumber( deviceSerialNumber );

    /* Initialize the PKCS11 session for cryptographic operation. */
    deviceInitializePKCS11Session( &p11Session );

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
}
