#ifndef SIMPLESAMPLEMQTT_H
#define SIMPLESAMPLEMQTT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IOTHUB_CLIENT_LL_HANDLE_DATA_TAG *IOTHUB_CLIENT_LL_HANDLE;
extern struct IOTHUB_CLIENT_LL_HANDLE_DATA_TAG *iotHubClientHandle;

uint8_t logTemp(void);
void simplesample_mqtt_run(void);
void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLESAMPLEMQTT_H */
