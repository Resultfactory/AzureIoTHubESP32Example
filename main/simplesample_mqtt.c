// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for
// full license information.

#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

#ifdef MBED_BUILD_TIMESTAMP
#include "certs.h"
#endif // MBED_BUILD_TIMESTAMP

//Azure IoT Hub component
#include "threadapi.h"
#include "platform.h"
#include "serializer.h"
#include "iothub_client_ll.h"
#include "iothubtransportmqtt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/heap_regions.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h" //wifi rssi
#include "esp_heap_alloc_caps.h"

#include "driver/adc.h"
extern uint8_t temprature_sens_read(void); // from librtc.a

#include "simplesample_mqtt.h"

static const char *EXAMPLE_CONNECTION_STRING = CONFIG_CONNECTION_STRING;
static const char *TAG = "simplesamplemqtt";

//Define the Model
BEGIN_NAMESPACE(WeatherStation);

DECLARE_MODEL(ContosoAnemometer,
			  WITH_DATA(ascii_char_ptr, DeviceId),
			  WITH_DATA(int, WindSpeed),
			  WITH_DATA(int, WifiRSSI),
			  WITH_DATA(int, Heap),
			  WITH_DATA(int, Heap32),
			  WITH_DATA(int, Temp),
			  WITH_DATA(int, Hall),
			  WITH_DATA(int, TickMS),
			  WITH_ACTION(Heap),
			  WITH_ACTION(Temp),
			  WITH_ACTION(TurnFanOff),
			  WITH_ACTION(SetAirResistance, int, Position));

END_NAMESPACE(WeatherStation);
ContosoAnemometer *myWeather;

struct IOTHUB_CLIENT_LL_HANDLE_DATA_TAG *iotHubClientHandle = NULL;

//Device explorer Messages to Device
//{"Name" : "Heap", "Parameters" : { }}
//{"Name" : "Temp", "Parameters" : { }}
//{"Name" : "TurnFanOff", "Parameters" : { }}
//{"Name" : "SetAirResistance", "Parameters" : { "Position" : 5 }}

EXECUTE_COMMAND_RESULT Heap(ContosoAnemometer *device)
{
	unsigned char *destination;
	size_t destinationSize;
	device->Heap = esp_get_free_heap_size();
	ESP_LOGD(TAG, "Heap: %d", device->Heap);
	if (SERIALIZE(&destination, &destinationSize, device->DeviceId,
				  device->Heap) != CODEFIRST_OK)
	{
		ESP_LOGE(TAG, "Failed to serialize");
	}
	else
	{
		sendMessage(iotHubClientHandle, destination, destinationSize);
	}
	return EXECUTE_COMMAND_SUCCESS;
}

uint8_t logTemp(void)
{
	return Temp(myWeather);
}

EXECUTE_COMMAND_RESULT Temp(ContosoAnemometer *device)
{
	unsigned char *destination;
	size_t destinationSize;
	device->Temp = temprature_sens_read();
	device->Hall = hall_sensor_read();
	device->Heap = esp_get_free_heap_size();
	device->TickMS = esp_log_timestamp();
	device->Heap32 = xPortGetFreeHeapSizeTagged(MALLOC_CAP_32BIT);

	wifi_ap_record_t wifidata;
	if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
	{
		device->WifiRSSI = wifidata.rssi;
	}
	else
	{
		device->WifiRSSI = 0;
	}

	ESP_LOGI(TAG, "Log Message; 'Temp: %d, Hall: %d, Heap: %d, Heap32: %d, WifiRSSI: %d'",
			 device->Temp, device->Hall, device->Heap, device->Heap32,
			 device->WifiRSSI);
	if (SERIALIZE(&destination, &destinationSize, device->DeviceId,
				  device->Temp, device->Hall, device->Heap, device->Heap32,
				  device->WifiRSSI, device->TickMS) != CODEFIRST_OK)
	{
		ESP_LOGE(TAG, "Failed to serialize");
	}
	else
	{
		sendMessage(iotHubClientHandle, destination, destinationSize);
	}
	return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT TurnFanOff(ContosoAnemometer *device)
{
	(void)device;
	ESP_LOGD(TAG, "Turning fan off.");
	return EXECUTE_COMMAND_SUCCESS;
}

EXECUTE_COMMAND_RESULT SetAirResistance(ContosoAnemometer *device, int Position)
{
	(void)device;
	ESP_LOGD(TAG, "Setting Air Resistance Position to %d.", Position);
	return EXECUTE_COMMAND_SUCCESS;
}

void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
	unsigned int messageTrackingId = (unsigned int)(uintptr_t)userContextCallback;

	ESP_LOGD(TAG, "Message Id: %u Received.", messageTrackingId);

	ESP_LOGD(TAG, "Result Call Back Called! Result is: %s ",
			 ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

	if (result == IOTHUB_CLIENT_CONFIRMATION_MESSAGE_TIMEOUT)
	{
		ESP_LOGD(TAG, "IOTHUB_CLIENT_CONFIRMATION_MESSAGE_TIMEOUT, DestroyClient");
	}
}

void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char *buffer, size_t size)
{
	static unsigned int messageTrackingId;
	IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
	if (messageHandle == NULL)
	{
		ESP_LOGE(TAG, "unable to create a new IoTHubMessage");
	}
	else
	{
		if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle,
										   sendCallback, (void *)(uintptr_t)messageTrackingId) !=
			IOTHUB_CLIENT_OK)
		{
			ESP_LOGE(TAG, "failed to hand over the message to IoTHubClient");
		}
		else
		{
			ESP_LOGD(TAG, "IoTHubClient accepted the message for delivery");
		}
		IoTHubMessage_Destroy(messageHandle);
	}
	free((unsigned char *)buffer);
	messageTrackingId++;
}

/*this function "links" IoTHub to the serialization library*/
static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
	IOTHUBMESSAGE_DISPOSITION_RESULT result;
	const unsigned char *buffer;
	size_t size;
	if (IoTHubMessage_GetByteArray(message, &buffer, &size) !=
		IOTHUB_MESSAGE_OK)
	{
		ESP_LOGE(TAG, "unable to IoTHubMessage_GetByteArray");
		result = EXECUTE_COMMAND_ERROR;
	}
	else
	{
		/*buffer is not zero terminated*/
		char *temp = malloc(size + 1);
		if (temp == NULL)
		{
			ESP_LOGE(TAG, "failed to malloc");
			result = EXECUTE_COMMAND_ERROR;
		}
		else
		{
			memcpy(temp, buffer, size);
			temp[size] = '\0';
			ESP_LOGD(TAG, "message: %s", temp);
			EXECUTE_COMMAND_RESULT executeCommandResult =
				EXECUTE_COMMAND(userContextCallback, temp);
			result =
				(executeCommandResult == EXECUTE_COMMAND_ERROR) ? IOTHUBMESSAGE_ABANDONED : (executeCommandResult == EXECUTE_COMMAND_SUCCESS) ? IOTHUBMESSAGE_ACCEPTED : IOTHUBMESSAGE_REJECTED;
			free(temp);
		}
	}
	return result;
}

void simplesample_mqtt_run(void)
{
	if (platform_init() != 0)
	{
		ESP_LOGE(TAG, "Failed to initialize platform.");
	}
	else
	{
		if (serializer_init(NULL) != SERIALIZER_OK)
		{
			ESP_LOGE(TAG, "Failed on serializer_init");
		}
		else
		{
			iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(EXAMPLE_CONNECTION_STRING, MQTT_Protocol);
			srand((unsigned int)time(NULL));
			int avgWindSpeed = 10;

			if (iotHubClientHandle == NULL)
			{
				ESP_LOGE(TAG, "Failed on IoTHubClient_LL_Create");
			}
			else
			{
#ifdef MBED_BUILD_TIMESTAMP
				// For mbed add the certificate information
				if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
				{
					ESP_LOGD(TAG, "failure to set option \"TrustedCerts\"");
				}
#endif // MBED_BUILD_TIMESTAMP

				myWeather = CREATE_MODEL_INSTANCE(WeatherStation, ContosoAnemometer);
				if (myWeather == NULL)
				{
					ESP_LOGE(TAG, "Failed on CREATE_MODEL_INSTANCE");
				}
				else
				{
					if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, IoTHubMessage, myWeather) != IOTHUB_CLIENT_OK)
					{
						ESP_LOGE(TAG, "unable to IoTHubClient_SetMessageCallback");
					}
					else
					{
						myWeather->DeviceId = "CLIENTID";
						myWeather->WindSpeed = avgWindSpeed + (rand() % 4 + 2);
						myWeather->Temp = temprature_sens_read();
						myWeather->Hall = hall_sensor_read();
						myWeather->Heap = esp_get_free_heap_size();
						myWeather->TickMS = esp_log_timestamp();
						myWeather->Heap32 =
							xPortGetFreeHeapSizeTagged(MALLOC_CAP_32BIT);

						wifi_ap_record_t wifidata;
						if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
						{
							myWeather->WifiRSSI = wifidata.rssi;
						}
						else
						{
							myWeather->WifiRSSI = 0;
						}

						unsigned char *destination;
						size_t destinationSize;
						if (SERIALIZE(&destination, &destinationSize,
									  myWeather->DeviceId, myWeather->WindSpeed,
									  myWeather->DeviceId, myWeather->Temp,
									  myWeather->Hall, myWeather->Heap, myWeather->Heap32,
									  myWeather->WifiRSSI, myWeather->TickMS) !=
							CODEFIRST_OK)
						{
							ESP_LOGE(TAG, "Failed to serialize");
						}
						else
						{
							IOTHUB_MESSAGE_HANDLE messageHandle =
								IoTHubMessage_CreateFromByteArray(destination,
																  destinationSize);
							if (messageHandle == NULL)
							{
								ESP_LOGE(TAG, "unable to create a new IoTHubMessage");
							}
							else
							{
								if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle,
																   messageHandle, sendCallback, (void *)1) !=
									IOTHUB_CLIENT_OK)
								{
									ESP_LOGE(TAG, "failed to hand over the message to IoTHubClient");
								}
								else
								{
									ESP_LOGD(TAG, "IoTHubClient accepted the message for delivery");
								}

								IoTHubMessage_Destroy(messageHandle);
							}
							free(destination);
						}

						/* wait for commands */
						while (1)
						{
							IoTHubClient_LL_DoWork(iotHubClientHandle);
							ThreadAPI_Sleep(100);
						}
					}
					DESTROY_MODEL_INSTANCE(myWeather);
				}
				IoTHubClient_LL_Destroy(iotHubClientHandle);
			}
			serializer_deinit();
		}
		platform_deinit();
	}
	vTaskDelete(NULL);
}
