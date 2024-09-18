#include "pal_queue.h"

/* Include for Unity framework. */
#include "unity.h"
#include "unity_fixture.h"

/*-----------------------------------------------------------*/

#define PAL_SYNC_QUEUE_TEST_ITEMS           20
#define PAL_SYNC_QUEUE_TEST_ITEM_SIZE       4

static iotshdPal_SyncQueue_t * pSyncQueue = NULL;

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
TEST_GROUP( Full_PalSyncQueueTest );


/**
 * @brief Test setup function for transport interface test.
 */
TEST_SETUP( Full_PalSyncQueueTest )
{
}

/**
 * @brief Test tear down function for transport interface test.
 */
TEST_TEAR_DOWN( Full_PalSyncQueueTest )
{
    if( pSyncQueue != NULL )
    {
        iotshdPal_syncQueueDelete( pSyncQueue );
        pSyncQueue = NULL;
    }
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncQueueTest, PalSyncQueue_CreateDeleteTest )
{
    size_t queueItemSize;
    uint32_t i;
    uint8_t queueItemArray[ 16 ];
    bool retStatus;

    for( queueItemSize = 1; queueItemSize <= 16; queueItemSize++ )
    {
        pSyncQueue = iotshdPal_syncQueueCreate( PAL_SYNC_QUEUE_TEST_ITEMS, queueItemSize );
        TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncQueue, "Can't create sync queue" ) ;

        for( i = 0; i < queueItemSize; i++ )
        {
            queueItemArray[ i ] = i;
        }

        retStatus = iotshdPal_syncQueueSend( pSyncQueue, queueItemArray, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue send return error." );
        
        retStatus = iotshdPal_syncQueueReceive( pSyncQueue, queueItemArray, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue receive return error." );

        for( i = 0; i < queueItemSize; i++ )
        {
            TEST_ASSERT_EQUAL_MESSAGE( queueItemArray[ i ], i, "Received queue item not equal" );
        }

        iotshdPal_syncQueueDelete( pSyncQueue );
        pSyncQueue = NULL;
    }
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncQueueTest, PalSyncQueue_SendReceiveOneItemTest )
{
    int i;
    int receiveItem;
    bool retStatus;

    pSyncQueue = iotshdPal_syncQueueCreate( PAL_SYNC_QUEUE_TEST_ITEMS, sizeof( int ) );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncQueue, "Can't create sync queue" ) ;

    for( i = 0; i < PAL_SYNC_QUEUE_TEST_ITEMS; i ++ )
    {
        retStatus = iotshdPal_syncQueueSend( pSyncQueue, &i, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue send return error." );

        retStatus = iotshdPal_syncQueueReceive( pSyncQueue, &receiveItem, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue receive return error." );

        TEST_ASSERT_EQUAL_MESSAGE( i, receiveItem, "Send and receive item is not equal." );
    }

    iotshdPal_syncQueueDelete( pSyncQueue );
    pSyncQueue = NULL;
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncQueueTest, PalSyncQueue_SendReceiveFulltest )
{
    int i;
    int receiveItem;
    bool retStatus;

    pSyncQueue = iotshdPal_syncQueueCreate( PAL_SYNC_QUEUE_TEST_ITEMS, sizeof( int ) );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncQueue, "Can't create sync queue" ) ;

    for( i = 0; i < PAL_SYNC_QUEUE_TEST_ITEMS; i ++ )
    {
        retStatus = iotshdPal_syncQueueSend( pSyncQueue, &i, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue send return error." );
    }

    /* Send should return error now. */
    retStatus = iotshdPal_syncQueueSend( pSyncQueue, &i, 100 );
    TEST_ASSERT_EQUAL_MESSAGE( false, retStatus, "Queue send should return error when queue full." );

    for( i = 0; i < PAL_SYNC_QUEUE_TEST_ITEMS; i ++ )
    {
        retStatus = iotshdPal_syncQueueReceive( pSyncQueue, &receiveItem, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue receive return error." );
    }

    /* Receive should return error now. */
    retStatus = iotshdPal_syncQueueReceive( pSyncQueue, &receiveItem, 100 );
    TEST_ASSERT_EQUAL_MESSAGE( false, retStatus, "Queue receive should return error." );

    iotshdPal_syncQueueDelete( pSyncQueue );
    pSyncQueue = NULL;
}

/*-----------------------------------------------------------*/

TEST( Full_PalSyncQueueTest, PalSyncQueue_SendTimeoutTest )
{
    int i;
    int receiveItem;
    bool retStatus;
    uint32_t timeoutMs;
    uint32_t testStartTime;
    uint32_t testEndTime;

    pSyncQueue = iotshdPal_syncQueueCreate( PAL_SYNC_QUEUE_TEST_ITEMS, sizeof( int ) );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncQueue, "Can't create sync queue" ) ;

    for( i = 0; i < PAL_SYNC_QUEUE_TEST_ITEMS; i ++ )
    {
        retStatus = iotshdPal_syncQueueSend( pSyncQueue, &i, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue send return error." );
    }

    /* Send should return error now. */
    for( timeoutMs = 0; timeoutMs < 1000; timeoutMs += 100 )
    {
        testStartTime = Clock_GetTimeMs();
        retStatus = iotshdPal_syncQueueSend( pSyncQueue, &i, timeoutMs );
        testEndTime = Clock_GetTimeMs();

        /* Verification. */
        TEST_ASSERT_EQUAL_MESSAGE( false, retStatus, "Queue send should return error when queue full." );
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32( timeoutMs, ( testEndTime - testStartTime ) );
    }

    iotshdPal_syncQueueDelete( pSyncQueue );
    pSyncQueue = NULL;
}

/*-----------------------------------------------------------*/

static void prvProducerThread( void * pParam )
{
    int i;
    int produceValue = *( ( int * )( pParam ) );
    bool retStatus;

    for( i = 0; i < PAL_SYNC_QUEUE_TEST_ITEMS; i++ )
    {
        retStatus = iotshdPal_syncQueueSend( pSyncQueue, &produceValue, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue send return error." );
    }
}

static void prvConsumerThread( void * pParam )
{
    int receiveItem;
    bool retStatus;
    uint32_t produceValueCounts[ 2 ] = { 0 };
    int i;

    for( i = 0; i < ( PAL_SYNC_QUEUE_TEST_ITEMS * 2 ); i++ )
    {
        /* Receive should return error now. */
        retStatus = iotshdPal_syncQueueReceive( pSyncQueue, &receiveItem, 100 );
        TEST_ASSERT_EQUAL_MESSAGE( true, retStatus, "Queue receive return error." );

        /* Counting the value. */
        TEST_ASSERT_LESS_THAN_INT( 2, receiveItem );
        produceValueCounts[ receiveItem ]++;
    }

    TEST_ASSERT_EQUAL_MESSAGE( produceValueCounts[ 0 ], PAL_SYNC_QUEUE_TEST_ITEMS, "Produce 0 message is not received" );
    TEST_ASSERT_EQUAL_MESSAGE( produceValueCounts[ 0 ], PAL_SYNC_QUEUE_TEST_ITEMS, "Produce 1 message is not received" );
}

TEST( Full_PalSyncQueueTest, PalSyncQueue_MultipleProducers )
{
    int i;
    int receiveItem;
    bool retStatus;
    int produceValues[ 2 ] = { 0, 1 };

    FRTestThreadHandle_t consumerTaskHandle;
    FRTestThreadHandle_t producerTaskHandle1, producerTaskHandle2;
    
    pSyncQueue = iotshdPal_syncQueueCreate( PAL_SYNC_QUEUE_TEST_ITEMS, sizeof( int ) );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pSyncQueue, "Can't create sync queue." ) ;

    /* Create the Consumer first. */
    consumerTaskHandle = FRTest_ThreadCreate( prvConsumerThread, NULL );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, consumerTaskHandle, "Can't create consumer." );

    /* Create the Producers. */
    producerTaskHandle1 = FRTest_ThreadCreate( prvProducerThread, &produceValues[ 0 ] );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, producerTaskHandle1, "Can't create producer." );
    producerTaskHandle2 = FRTest_ThreadCreate( prvProducerThread, &produceValues[ 1 ] );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, producerTaskHandle2, "Can't create producer." );

    /* Verify the test result. */
    FRTest_ThreadTimedJoin( consumerTaskHandle, 1000 );
    FRTest_ThreadTimedJoin( producerTaskHandle1, 1000 );
    FRTest_ThreadTimedJoin( producerTaskHandle2, 1000 );

    iotshdPal_syncQueueDelete( pSyncQueue );
    pSyncQueue = NULL;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for transport interface test against echo server.
 */
TEST_GROUP_RUNNER( Full_PalSyncQueueTest )
{
    RUN_TEST_CASE( Full_PalSyncQueueTest, PalSyncQueue_CreateDeleteTest );
    RUN_TEST_CASE( Full_PalSyncQueueTest, PalSyncQueue_SendReceiveOneItemTest );
    RUN_TEST_CASE( Full_PalSyncQueueTest, PalSyncQueue_SendReceiveFulltest );

    RUN_TEST_CASE( Full_PalSyncQueueTest, PalSyncQueue_SendTimeoutTest );

    RUN_TEST_CASE( Full_PalSyncQueueTest, PalSyncQueue_MultipleProducers );
}

/*-----------------------------------------------------------*/

int RunPalSyncQueueTest( void )
{
    int status = -1;

    /* Initialize unity. */
    UnityFixture.Verbose = 1;
    UnityFixture.GroupFilter = 0;
    UnityFixture.NameFilter = 0;
    UnityFixture.RepeatCount = 1;
    UNITY_BEGIN();

    /* Run the test group. */
    RUN_TEST_GROUP( Full_PalSyncQueueTest );

    status = UNITY_END();

    return status;
}

/*-----------------------------------------------------------*/

int main( int argc, char** argv )
{
    RunPalSyncQueueTest();
    
    return 0;
}