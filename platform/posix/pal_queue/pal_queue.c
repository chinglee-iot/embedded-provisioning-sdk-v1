#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "pal_queue.h"

struct iotshdPal_SyncQueue
{
    uint8_t * pBuffer;                  /**< Points to the internal buffer. */

    uint32_t numberOfQueueItems;        /**< Number of queue items. */    
    uint32_t queueItemSize;             /**< Queue item size. */ 

    uint32_t itemsInQueue;              /**< Items in queue. */ 

    uint32_t enqueueIndex;              /**< enqueue index. */
    uint32_t dequeueIndex;              /**< dequeue index. */

    pthread_mutex_t mutex;              /**< Mutex for queue operation. */ 

    pthread_cond_t condEnqueue;         /**< Condition for enqueue event notification. */ 
    pthread_cond_t condDequeue;         /**< Condition for dequeue event notification. */ 
};

iotshdPal_SyncQueue_t * iotshdPal_syncQueueCreate( uint32_t numberOfQueueItems,
                                                   size_t queueItemSize )
{
    iotshdPal_SyncQueue_t * pSyncQueue = NULL;

    if( numberOfQueueItems == 0 )
    {
    }
    else if( queueItemSize == 0 )
    {
    }
    else
    {
        /* Initialize the queue structure. */
        pSyncQueue = ( iotshdPal_SyncQueue_t * ) malloc( sizeof( iotshdPal_SyncQueue_t ) );
        memset( pSyncQueue, 0, sizeof( iotshdPal_SyncQueue_t ) );

        /* Create the buffer. */
        pSyncQueue->pBuffer = ( uint8_t * ) malloc( numberOfQueueItems * queueItemSize );
        pSyncQueue->numberOfQueueItems = numberOfQueueItems;
        pSyncQueue->queueItemSize = queueItemSize;    

        /* Create the mutex and condition. */
        pthread_mutex_init( &pSyncQueue->mutex, NULL);
        pthread_cond_init( &pSyncQueue->condEnqueue, NULL);
        pthread_cond_init( &pSyncQueue->condDequeue, NULL);
    }

    return pSyncQueue;
}

void iotshdPal_syncQueueDelete( iotshdPal_SyncQueue_t * pSyncQueue )
{
    if( pSyncQueue != NULL )
    {
        if( pSyncQueue->pBuffer != NULL )
        {
            free( pSyncQueue->pBuffer );
        }
        free( pSyncQueue );
    }
}

bool iotshdPal_syncQueueSend( iotshdPal_SyncQueue_t * pSyncQueue ,
                              void * pQueueItemToSend,
                              uint32_t blockTimeMs )
{
    uint32_t queueIndex;
    uint8_t * pEnqueuePos;
    
    struct timeval now;
    struct timespec timeout;
    int ret = 0;
    bool retStatus = false;

    pthread_mutex_lock( &( pSyncQueue->mutex ) );

    /* Check if the queue if full. */
    while( pSyncQueue->itemsInQueue == pSyncQueue->numberOfQueueItems )
    {
        gettimeofday( &now, NULL );
        timeout.tv_sec = now.tv_sec + ( blockTimeMs / 1000 );
        timeout.tv_nsec = ( now.tv_usec + ( blockTimeMs % 1000 ) * 1000 ) * 1000;

        if( timeout.tv_nsec >= 1000000000 )
        {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }

        ret = pthread_cond_timedwait( &pSyncQueue->condDequeue, &pSyncQueue->mutex, &timeout );

        /* Break on error. */
        if( ret != 0 )
        {
            break;
        }
    }

    /* The queue has free slot. */
    if( ret == 0 )
    {
        pSyncQueue->itemsInQueue = pSyncQueue->itemsInQueue + 1;
        queueIndex = pSyncQueue->enqueueIndex;
        pSyncQueue->enqueueIndex = ( pSyncQueue->enqueueIndex + 1 ) % pSyncQueue->numberOfQueueItems;

        pEnqueuePos = ( uint8_t * )( &pSyncQueue->pBuffer[ queueIndex * pSyncQueue->queueItemSize ] );
        memcpy( pEnqueuePos, pQueueItemToSend, pSyncQueue->queueItemSize );
        
        retStatus = true;
    }

    pthread_mutex_unlock( &( pSyncQueue->mutex ) );
    if( retStatus == true )
    {
        /* Notify the enqueue condition. */
        pthread_cond_broadcast( &( pSyncQueue->condEnqueue ) );
    }

    return retStatus;
}

bool iotshdPal_syncQueueReceive( iotshdPal_SyncQueue_t * pSyncQueue,
                                 void * pvBuffer,
                                 uint32_t blockTimeMs )
{
    uint8_t * pDnqueuePos;
    struct timeval now;
    struct timespec timeout;
    int ret = 0;
    bool retStatus = false;

    pthread_mutex_lock( &( pSyncQueue->mutex ) );

    /* Check if the queue if empty. */
    while( pSyncQueue->itemsInQueue == 0 )
    {
        gettimeofday( &now, NULL );
        timeout.tv_sec = now.tv_sec + ( blockTimeMs / 1000 );
        timeout.tv_nsec = ( now.tv_usec + ( blockTimeMs % 1000 ) * 1000 ) * 1000;

        if( timeout.tv_nsec >= 1000000000 )
        {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }

        ret = pthread_cond_timedwait( &pSyncQueue->condEnqueue, &pSyncQueue->mutex, &timeout );

        /* Break on other error. */
        if( ret != 0 )
        {
            break;
        }
    }

    /* The queue has data. */
    if( ret == 0 )
    {
        pDnqueuePos = ( uint8_t * )( &pSyncQueue->pBuffer[ pSyncQueue->dequeueIndex * pSyncQueue->queueItemSize ] );
        memcpy( pvBuffer, pDnqueuePos, pSyncQueue->queueItemSize );

        pSyncQueue->itemsInQueue = pSyncQueue->itemsInQueue - 1;
        pSyncQueue->dequeueIndex = ( pSyncQueue->dequeueIndex + 1 ) % pSyncQueue->numberOfQueueItems;
        
        retStatus = true;
    }

    pthread_mutex_unlock( &( pSyncQueue->mutex ) );
    if( retStatus == true )
    {
        /* Notify the dequeue condition. */
        pthread_cond_broadcast( &( pSyncQueue->condDequeue ) );
    }

    return retStatus;
}
