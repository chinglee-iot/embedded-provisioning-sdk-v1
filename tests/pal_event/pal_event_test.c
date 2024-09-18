#include "pal_event.h"

/* Include for Unity framework. */
#include "unity.h"
#include "unity_fixture.h"

/*-----------------------------------------------------------*/

static iotshdPal_SyncEvent_t * pSyncEvent = NULL;
static iotshdPal_SyncEvent_t * pSyncEvent2 = NULL;

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

/*-----------------------------------------------------------*/

extern uint32_t Clock_GetTimeMs( void );

/*-----------------------------------------------------------*/

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

int FRTest_ThreadTimedJoin( FRTestThreadHandle_t threadHandle, uint32_t timeoutMs )
{
    pthread_t * pThreadid = threadHandle;

    ( void ) timeoutMs;

    pthread_join( *pThreadid, NULL );
    return 0;
}

/*-----------------------------------------------------------*/


/**
 * @brief Test group for transport interface test.
 */
TEST_GROUP( Full_PalSyncEventTest );


/**
 * @brief Test setup function for transport interface test.
 */
TEST_SETUP( Full_PalSyncEventTest )
{
}

/**
 * @brief Test tear down function for transport interface test.
 */
TEST_TEAR_DOWN( Full_PalSyncEventTest )
{
    if( pSyncEvent != NULL )
    {
        iotshdPal_syncEventDelete( pSyncEvent );
        pSyncEvent = NULL;
    }

    if( pSyncEvent2 != NULL )
    {
        iotshdPal_syncEventDelete( pSyncEvent2 );
        pSyncEvent2 = NULL;
    }
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncEventTest, PalSyncEvent_CreateDeleteTest )
{
    pSyncEvent = iotshdPal_syncEventCreate();
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncEvent, "Can't create sync event" );

    iotshdPal_syncEventDelete( pSyncEvent );
    pSyncEvent = NULL;
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncEventTest, PalSyncEvent_WaitTimeoutTest )
{
    uint32_t timeoutMs;
    uint32_t testStartTime, testEndTime;
    bool retStatus;

    pSyncEvent = iotshdPal_syncEventCreate();
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncEvent, "Can't create sync event" );

    /* Send should return error now. */
    for( timeoutMs = 0; timeoutMs < 1000; timeoutMs += 100 )
    {
        testStartTime = Clock_GetTimeMs();
        retStatus = iotshdPal_syncEventWait( pSyncEvent, timeoutMs );
        testEndTime = Clock_GetTimeMs();

        /* Verification. */
        TEST_ASSERT_EQUAL_MESSAGE( false, retStatus, "Event wait should return error when queue full." );
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32( timeoutMs, ( testEndTime - testStartTime ) );
    }

    iotshdPal_syncEventDelete( pSyncEvent );
    pSyncEvent = NULL;
}

/*-----------------------------------------------------------*/

static void prvWaitEventThread( void * pParam )
{
    bool retStatus;
    retStatus = iotshdPal_syncEventWait( pSyncEvent, 10000 );
    TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Sync event is not waited." );

    iotshdPal_syncEventSet( pSyncEvent2 );
}

TEST( Full_PalSyncEventTest, PalSyncEvent_SendEventFromOtherThread )
{
    bool retStatus;
    FRTestThreadHandle_t waitEventThreadHandle;

    pSyncEvent = iotshdPal_syncEventCreate();
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncEvent, "Can't create sync event" );

    pSyncEvent2 = iotshdPal_syncEventCreate();
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncEvent, "Can't create sync event" );

    /* Create one thread to wait for the event. */
    waitEventThreadHandle = FRTest_ThreadCreate( prvWaitEventThread, NULL );
    
    /* Sleep for some time for the event. */
    sleep( 5 );
    
    /* Set the event. */
    iotshdPal_syncEventSet( pSyncEvent );

    /* sleep for some time for the event. */
    sleep( 5 );

    retStatus = iotshdPal_syncEventWait( pSyncEvent2, 10000 );
    TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Sync event is not waited." );

    /* Wait event again should timeout. */
    retStatus = iotshdPal_syncEventWait( pSyncEvent2, 1000 );
    TEST_ASSERT_EQUAL_MESSAGE( false, retStatus, "Sync event is waited." );

    /* Wait the created thread join. */
    FRTest_ThreadTimedJoin( waitEventThreadHandle, 1000 );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for transport interface test against echo server.
 */
TEST_GROUP_RUNNER( Full_PalSyncEventTest )
{
    RUN_TEST_CASE( Full_PalSyncEventTest, PalSyncEvent_CreateDeleteTest );
    RUN_TEST_CASE( Full_PalSyncEventTest, PalSyncEvent_WaitTimeoutTest );
    
    RUN_TEST_CASE( Full_PalSyncEventTest, PalSyncEvent_SendEventFromOtherThread );
}

/*-----------------------------------------------------------*/

int RunPalSyncEventTest( void )
{
    int status = -1;

    /* Initialize unity. */
    UnityFixture.Verbose = 1;
    UnityFixture.GroupFilter = 0;
    UnityFixture.NameFilter = 0;
    UnityFixture.RepeatCount = 1;
    UNITY_BEGIN();

    /* Run the test group. */
    RUN_TEST_GROUP( Full_PalSyncEventTest );

    status = UNITY_END();

    return status;
}

/*-----------------------------------------------------------*/

int main( int argc, char** argv )
{
    RunPalSyncEventTest();
    
    return 0;
}