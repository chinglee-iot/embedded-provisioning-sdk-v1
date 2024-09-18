#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "pal_event.h"

struct iotshdPal_SyncEvent
{
    bool eventStatus;
    pthread_mutex_t mutex;              /**< Mutex for queue operation. */ 
    pthread_cond_t cond;                /**< Condition for event notification. */ 
};

iotshdPal_SyncEvent_t * iotshdPal_syncEventCreate( void )
{
    iotshdPal_SyncEvent_t * pSyncEvent;
    pSyncEvent = ( iotshdPal_SyncEvent_t * ) malloc( sizeof( iotshdPal_SyncEvent_t ) );

    if( pSyncEvent != NULL )
    {
        memset( pSyncEvent, 0, sizeof( iotshdPal_SyncEvent_t ) );

        pthread_mutex_init( &pSyncEvent->mutex, NULL);
        pthread_cond_init( &pSyncEvent->cond, NULL);
    }

    return pSyncEvent;
}

void iotshdPal_syncEventDelete( iotshdPal_SyncEvent_t * pSyncEvent )
{
    if( pSyncEvent != NULL )
    {
        free( pSyncEvent );
    }
}

bool iotshdPal_syncEventWait( iotshdPal_SyncEvent_t * pSyncEvent, uint32_t blockTimeMs )
{
    struct timeval now;
    struct timespec timeout;
    int ret = 0;
    bool retStatus = false;

    pthread_mutex_lock( &( pSyncEvent->mutex ) );

    while( pSyncEvent->eventStatus == false )
    {
        gettimeofday( &now, NULL );
        timeout.tv_sec = now.tv_sec + ( blockTimeMs / 1000 );
        timeout.tv_nsec = ( now.tv_usec + ( blockTimeMs % 1000 ) * 1000 ) * 1000;

        if( timeout.tv_nsec >= 1000000000 )
        {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }

        ret = pthread_cond_timedwait( &pSyncEvent->cond, &pSyncEvent->mutex, &timeout );
        if( ret != 0 )
        {
            break;
        }
    }

    if( ret == 0 )
    {
        pSyncEvent->eventStatus = false;
        retStatus = true;
    }

    pthread_mutex_unlock( &( pSyncEvent->mutex ) );

    return retStatus;
}

void iotshdPal_syncEventSet( iotshdPal_SyncEvent_t * pSyncEvent )
{
    pthread_mutex_lock( &( pSyncEvent->mutex ) );
    pSyncEvent->eventStatus = true;
    pthread_mutex_unlock( &( pSyncEvent->mutex ) );

    pthread_cond_broadcast( &pSyncEvent->cond );
}
