#ifndef PAL_QUEUE_H_
#define PAL_QUEUE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * @brief platform defined queue structure.
 */
struct iotshdPal_SyncQueue;
typedef struct iotshdPal_SyncQueue iotshdPal_SyncQueue_t;

/**
 * @brief Platform queue create API.
 *
 * @param numberOfQueueItems Maximum number of queue item can be enqueued.
 * @param queueItemSize Size of the queue item.
 *
 * @return Pointer to created iotshdPal_SyncQueue_t structure when success. Otherwise,
 * return NULL to indicate queue creation failure.
 */
iotshdPal_SyncQueue_t * iotshdPal_syncQueueCreate( uint32_t numberOfQueueItems,
                                                   size_t queueItemSize );

/**
 * @brief Platform queue delete API.
 *
 * @param pSyncQueue pointer to iotshd_deviceQueue_t structure to be deleted.
 */
void iotshdPal_syncQueueDelete( iotshdPal_SyncQueue_t * pSyncQueue );

/**
 * @brief Platform queue send API.
 *
 * @param pSyncQueue pointer iotshd_deviceQueue_t structure.
 * @param pQueueItemToSend pointer to queue item to be sent.
 * @param blockTimeMs Maximum block time to send the item in miliseconds.
 *
 * @return true to indicate send to queue success. false to indicate failure.
 */
bool iotshdPal_syncQueueSend( iotshdPal_SyncQueue_t * pSyncQueue ,
                              void * pQueueItemToSend,
                              uint32_t blockTimeMs );

/**
 * @brief Platform queue receive API.
 *
 * @param pSyncQueue pointer iotshd_deviceQueue_t structure.
 * @param pvBuffer pointer to buffer for receiving from queue.
 * @param blockTimeMs Maximum block time to send the item in miliseconds.
 *
 * @return true to indicate receive from queue success. false to indicate failure.
 */
bool iotshdPal_syncQueueReceive( iotshdPal_SyncQueue_t * pSyncQueue,
                                 void * pvBuffer,
                                 uint32_t blockTimeMs );

#endif

