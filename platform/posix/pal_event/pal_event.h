#ifndef PAL_EVENT_H_
#define PAL_EVENT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief platform defined event structure.
 */
typedef struct iotshdPal_SyncEvent iotshdPal_SyncEvent_t;

/**
 * @brief Platform queue create API.
 *
 * @return Pointer to created iotshdPal_SyncEvent_t structure when success. Otherwise,
 * return NULL to indicate event creation failure.
 */
iotshdPal_SyncEvent_t * iotshdPal_syncEventCreate( void );

/**
 * @brief Platform event delete API.
 *
 * @param pSyncEvent pointer to iotshdPal_SyncEvent structure to be deleted.
 */
void iotshdPal_syncEventDelete( iotshdPal_SyncEvent_t * pSyncEvent );

/**
 * @brief Platform event wait API.
 *
 * @param pSyncEvent pointer iotshdPal_SyncEvent structure.
 * @param blockTimeMs Maximum block time to wait for the event in miliseconds.
 *
 * @return true to indicate the notification within blockTimeMs. Otherwise, return false.
 */
bool iotshdPal_syncEventWait( iotshdPal_SyncEvent_t * pSyncEvent, uint32_t blockTimeMs );

/**
 * @brief Platform event set API.
 *
 * @param pSyncEvent pointer iotshdPal_SyncEvent_t structure.
 */
void iotshdPal_syncEventSet( iotshdPal_SyncEvent_t * pSyncEvent );

#endif
