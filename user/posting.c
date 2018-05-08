/*
* ----------------------------------------------------------------------------
* The MIT License (MIT)
*
* Copyright (c) 2016 - Matthias Jentsch
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
* ----------------------------------------------------------------------------
*/

#include "user_interface.h"
#include "osapi.h"
#include "mem.h"
#include <espmissingincludes.h>
#include <ultrasonicmeter.h>
#include <calculator.h>
#include <powermanagement.h>
#include <httpclient.h>
#include <mqtt.h>
#include <configuration.h>
#include <log.h>

// Timeout timer; if the posting last too long we cancel the posting process
static ETSTimer posting_timeoutTimer;
// MQTT client
static MQTT_Client posting_mqttClient;
// MQTT published value counter (countdown; if zero the all values are published)
static int posting_mqttPublishCountdown;
// if TRUE thingspeak posting is done or not needed at all
static int posting_thingspeakDone;
// if TRUE MQTT posting is done or not needed at all
static int posting_mqttDone;

// callback if the timeout timer is elapsed
static void ICACHE_FLASH_ATTR posting_timeoutTimerTick(void *arg)
{
	os_printf("Sending the data lasts too long! Canceling ...\n");
	os_timer_disarm(&posting_timeoutTimer);
	powermanagement_postingCanceled();
	powermanagement_deepSleep();
}

// called from the http client module after the posting was finished
static void ICACHE_FLASH_ATTR posting_finished(char * response, int http_status, char * full_response)
{
	os_printf("http_status=%d\n", http_status);
	if (http_status != HTTP_STATUS_GENERIC_ERROR)
	{
		os_printf("\nstrlen(full_response)=%d\n", strlen(full_response));
		os_printf("\nresponse=%s<EOF>\n", response);
		os_printf("\nData sent!\n");
		powermanagement_measurementPosted();
	}
	// go to sleep if also the MQTT posting has finished
	posting_thingspeakDone = TRUE;
	if (posting_mqttDone == TRUE)
	{
		powermanagement_deepSleep();
	}
}

// called after the MQTT client is connected to the MQTT broker
static void ICACHE_FLASH_ATTR posting_mqttClientConnected(uint32_t *args)
{
	char topic[256];
	char data[256];
	
	MQTT_Client* client = (MQTT_Client*)args;
	os_printf("MQTT: Client connected!\n");

	// publish all three water level values
	posting_mqttPublishCountdown = 3;

	os_sprintf(topic, "%s/centimeter", configuration_getMqttTopic());
	os_sprintf(data, "%d", (int)calculator_getCentimeter());
	os_printf("MQTT: Publishing %s => %s\n", topic, data);
	MQTT_Publish(client, topic, data, strlen(data), 0, TRUE);
	
	os_sprintf(topic, "%s/liter", configuration_getMqttTopic());
	os_sprintf(data, "%d", (int)calculator_getLiter());
	os_printf("MQTT: Publishing %s => %s\n", topic, data);
	MQTT_Publish(client, topic, data, strlen(data), 0, TRUE);

	os_sprintf(topic, "%s/percent", configuration_getMqttTopic());
	os_sprintf(data, "%d", (int)calculator_getPercent());
	os_printf("MQTT: Publishing %s => %s\n", topic, data);
	MQTT_Publish(client, topic, data, strlen(data), 0, TRUE);
}

// called after the MQTT client has published one value
static void ICACHE_FLASH_ATTR posting_mqttPublished(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	os_printf("MQTT: Published\n");
	// one value published; all values published?
	posting_mqttPublishCountdown--;
	if (posting_mqttPublishCountdown == 0)
	{
		os_printf("All data published!\n");
		os_printf("MQTT: Disconnecting\n");
		MQTT_Disconnect(client);
	}
}

// called after data was received from the MQTT broker; currently not used
static void ICACHE_FLASH_ATTR posting_mqttDataReceived(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char* topicBuffer = (char*)os_zalloc(topic_len + 1);
	char* dataBuffer = (char*)os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuffer, topic, topic_len);
	topicBuffer[topic_len] = 0;

	os_memcpy(dataBuffer, data, data_len);
	dataBuffer[data_len] = 0;

	os_printf("Receive topic: %s, data: %s \n", topicBuffer, dataBuffer);
	os_free(topicBuffer);
	os_free(dataBuffer);
}

// called after the MQTT client has disconnected from the MQTT broker
static void ICACHE_FLASH_ATTR posting_mqttClientDisconnected(uint32_t *args)
{
	os_printf("MQTT: Client disconnected!\n");
	if (posting_mqttPublishCountdown == 0)
	{
		powermanagement_measurementPosted();
	}
	// go to sleep if also the Thingspeak posting has finished
	posting_mqttDone = TRUE;
	if (posting_thingspeakDone == TRUE)
	{
		powermanagement_deepSleep();
	}
}

// initialize MQTT part
void ICACHE_FLASH_ATTR posting_initializeMqtt()
{
	// MQTT connection configuration
	MQTT_InitConnection(&posting_mqttClient, configuration_getMqttServer(), configuration_getMqttPort(), FALSE);
	// MQTT client configuration
	MQTT_InitClient(&posting_mqttClient, configuration_getMqttClientName(), configuration_getMqttUsername(), configuration_getMqttPassword(), 0, TRUE);
	// set callbacks
	MQTT_OnConnected(&posting_mqttClient, posting_mqttClientConnected);
	MQTT_OnDisconnected(&posting_mqttClient, posting_mqttClientDisconnected);
	MQTT_OnPublished(&posting_mqttClient, posting_mqttPublished);
	MQTT_OnData(&posting_mqttClient, posting_mqttDataReceived);
}

// called after the connection to the access point is finished; starts the posting of the data via http client
void ICACHE_FLASH_ATTR posting_start(System_Event_t *evt)
{
	// do we got an IP?
	os_printf("event %x\n", evt->event);
	if (evt->event == EVENT_STAMODE_GOT_IP)
	{
		if (powermanagement_shouldPostMeasurement() == TRUE)
		{
			os_printf("Ready to send the data!\n");
			wifi_station_set_hostname(configuration_getHostname());

			calculator_calculateNewValues(powermanagement_getLastMeasurement());

			posting_thingspeakDone = configuration_shouldPostToThingspeak() ? FALSE : TRUE;
			posting_mqttDone = configuration_shouldPostToMqtt() ? FALSE : TRUE;
			
			// If needed: Send data to Thingspeak
			if (configuration_shouldPostToThingspeak() == TRUE)
			{
				os_printf("Sending to Thingspeak...\n");
				char url[256];
				os_sprintf(url, THINGSPEAK_URL, configuration_getThingspeakServerUrl(), configuration_getThingspeakApiKey(), (int)calculator_getCentimeter(), (int)calculator_getLiter(), (int)calculator_getPercent());
				os_printf("%s\n", url);
				http_get(url, "", posting_finished);
			}
			// If needed: Send data to MQTT broker
			if (configuration_shouldPostToMqtt() == TRUE)
			{
				os_printf("Sending to MQTT broker...\n");
				MQTT_Connect(&posting_mqttClient);
			}

			// now we are connected to the internet! => post also the log data
			log_post();
		}
		else if (powermanagement_shouldPostLog() == TRUE)
		{
			log_post();
		}
	}
}

// Timeout timer; if the posting last too long we cancel the posting process
void ICACHE_FLASH_ATTR posting_startTimeoutTimer()
{
	// enable the timeout timer as watchdog; after 60 seconds the posting should be done
	os_timer_disarm(&posting_timeoutTimer);
	os_timer_setfn(&posting_timeoutTimer, posting_timeoutTimerTick, NULL);
	os_timer_arm(&posting_timeoutTimer, POST_MEASUREMENT_TIMEOUT * 1000, 0);
}

// called after the ultrasonic measurement is finished; checks if the date should pe posted
void ICACHE_FLASH_ATTR posting_checkIfPostNeeded()
{
	os_printf("Measurement finished!\n");
	float waterLevel = ultrasonicMeter_getWaterLevel();
	os_printf("Water level = %d mm\n", (int)waterLevel);
	os_printf("Quality = %d\n", ultrasonicMeter_getEchoQuality());
	if (powermanagement_checkCurrentMeasurement(waterLevel) == TRUE)
	{
		os_printf("\nData should be send!\n");
	}
	else
	{
		os_printf("\nSending data not needed!\n");
	}
	// go to sleep
	powermanagement_deepSleep();
}
