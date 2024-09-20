
#ifndef MQTT_AGENT_H_
#define MQTT_AGENT_H_

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "core_pkcs11.h"

#include "pal_event.h"
#include "pal_queue.h"

typedef struct iotshdDev_MQTTAgentQueueItem
{
    MQTTPublishInfo_t publishInfo;
    uint8_t * topicPayloadBuffer;
    size_t topicPayloadBufferSize;
} iotshdDev_MQTTAgentQueueItem_t;

typedef struct iotshdDev_MQTTAgentUserContext
{
    MQTTStatus_t xReturnStatus;
    iotshdPal_SyncEvent_t * pSyncEvent;

    iotshdPal_SyncQueue_t * pIncommingPublishQueue;
    iotshdPal_SyncQueue_t * pFreePublishMessageQueue;
    uint32_t queueSize;

    iotshdDev_MQTTAgentQueueItem_t * pQueueItems;
} iotshdDev_MQTTAgentUserContext_t;

/**
 * @brief Callback function called when receiving a publish.
 *
 * @param[in] pvIncomingPublishCallbackContext The incoming publish callback context.
 * @param[in] pxPublishInfo Deserialized publish information.
 */
typedef void (* IncomingPubCallback_t )( void * pvIncomingPublishCallbackContext,
                                         MQTTPublishInfo_t * pxPublishInfo );


/**
 * @brief MQTT Agent init function.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentInit( NetworkContext_t * pxNetworkContext );

/**
 * @brief MQTT Agent init function.
 *
 * @param pNetworkContext pointer to user defined network context.
 * @param pSmarthomeEndpoint pointer to smart home endpoint.
 * @param pxSession User created PKCS11 session.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentThreadLoop( NetworkContext_t * pNetworkContext,
                                            const char * pSmarthomeEndpoint,
                                            CK_SESSION_HANDLE * pxSession );

/**
 * @brief MQTT Agent thread loop stop function.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentStop( void );

/**
 * @brief MQTT Agent create user context. User context keeps the data structure
 * used for synchrounous MQTT operations and incomming publish queue.
 *
 * @param incommingPublishQueueSize Maximum number of queue item in incomming publish
 * queue.
 *
 * @return Return pointer to created iotshdDev_MQTTAgentUserContext_t when success.
 * Otherwise, NULL is returned.
 */
iotshdDev_MQTTAgentUserContext_t * iotshdDev_MQTTAgentCreateUserContext( uint32_t incommingPublishQueueSize );

/**
 * @brief MQTT Agent delete user context.
 *
 * @param pUserContext user context to be deleted.
 */
void iotshdDev_MQTTAgentDeleteUserContext( iotshdDev_MQTTAgentUserContext_t * pUserContext );

/**
 * @brief MQTT Agent synchronous publish function.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pPublishInfo publish info.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                         MQTTPublishInfo_t * pPublishInfo,
                                         uint32_t blockTimeMs );

/**
 * @brief MQTT Agent synchronous subscription function. Qos1 will be used in this function.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pcTopicFilterString Topic filter of the subscription.
 * @param usTopicFilterLength Topic filter length.
 * @param pxIncomingPublishCallback callback function for incomming publish.
 * @param pvIncomingPublishCallbackContext context passed to callback function.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentAddSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                 const char * pcTopicFilterString,
                                                 uint16_t usTopicFilterLength,
                                                 IncomingPubCallback_t pxIncomingPublishCallback,
                                                 void * pvIncomingPublishCallbackContext,
                                                 uint32_t blockTimeMs );

/**
 * @brief MQTT Agent synchronous unsubscription function. Qos1 will be used in this function.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pcTopicFilterString Topic filter of the unsubscription.
 * @param usTopicFilterLength Topic filter length.
 * @param pxIncomingPublishCallback callback function for incomming publish.
 * @param pvIncomingPublishCallbackContext context passed to callback function.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentRemoveSubscription( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                    const char * pcTopicFilterString,
                                                    uint16_t usTopicFilterLength,
                                                    IncomingPubCallback_t pxIncomingPublishCallback,
                                                    void * pvIncomingPublishCallbackContext,
                                                    uint32_t blockTimeMs );

/**
 * @brief MQTT Agent subscrition with queue.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pcTopicFilterString Topic filter of the unsubscription.
 * @param usTopicFilterLength Topic filter length.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentAddSubscriptionWithQueue( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                          const char * pcTopicFilterString,
                                                          uint16_t usTopicFilterLength,
                                                          uint32_t blockTimeMs );

/**
 * @brief MQTT Agent unsubscrition with queue.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pcTopicFilterString Topic filter of the unsubscription.
 * @param usTopicFilterLength Topic filter length.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return MQTTSuccess to indicate success. Other value to indicate error.
 */
MQTTStatus_t iotshdDev_MQTTAgentRemoveSubscriptionWithQueue( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                             const char * pcTopicFilterString,
                                                             uint16_t usTopicFilterLength,
                                                             uint32_t blockTimeMs );

/**
 * @brief MQTT Agent dequeue incomming publish from queue.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param blockTimeMs Maximum block time to wait for operation complete.
 *
 * @return Return Pointer to queue item when success. Otherwise, return NULL to indicate error.
 */
iotshdDev_MQTTAgentQueueItem_t * iotshdDev_MQTTAgentDequeueIncommingPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                                                             uint32_t blockTimeMs );

/**
 * @brief MQTT Agent free queue item.
 *
 * @param pUserContext pointer to MQTT Agent user context.
 * @param pQueueItem pointer to the queue item to be freed. 
 * @param freeBuffer Free topic and payload buffer. The topic and payload buffer cloud
 * be reused for next incomming message if not freed.
 */
void iotshdDev_MQTTAgentFreeIncommingPublish( iotshdDev_MQTTAgentUserContext_t * pUserContext,
                                              iotshdDev_MQTTAgentQueueItem_t * pQueueItem,
                                              bool freeBuffer );

#endif
