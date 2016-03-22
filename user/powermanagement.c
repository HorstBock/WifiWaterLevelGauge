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

#include "osapi.h"
#include "user_interface.h"
#include "math.h"
#include <espmissingincludes.h>
#include <io.h>
#include <configuration.h>
#include <log.h>
#include <powermanagement.h>

// the magic number to check if the data in rtc memory is valid
#define RTC_MAGIC 0x5aa5
// const for invalid water level
#define LAST_MEASURED_WATER_LEVEL_INVALID -10000.0
// start address for the data structure in RTC memory; start of user data
#define RTC_DATA_ADDRESS 64

// data that will be stored into the RTC memory; this data survice the deep sleep
typedef struct
{
	unsigned short magic;	// if not DEEP_SLEEP_IS_INITIALIZED then the data in the struct is not valid
	float lastMeasuredWaterLevel;	// the last measured water level in mm
	unsigned short postUnchangedMeasurementCountDown;	// the water level was posted to the internet this amount of seconds before
	unsigned char shoudlEnterConfigurationMode; // set to TRUE if the configuration mode should be entered
	unsigned char shouldDoMeasurement;	// set to TRUE if the program should do a water level measurement; and don't post the data to the internet
	unsigned char shouldPostMeasurement;	// set to TRUE if the program should post the measured data to the internet; and don't do a water level measurement
	unsigned char shouldPostLog;	// set to TRUE if the program should post the log data to the internet; and don't do a water level measurement
	unsigned int nextLogBytePointer;	// points to the next log byte; relative to the beginning of the log; starts with 0
} DeepSleepSurvivalData;

// the instance of the data
static DeepSleepSurvivalData powermanagement_data;

// read the data structure from RTC memory or init the data if the data in RTC memory is not valid
unsigned char ICACHE_FLASH_ATTR powermanagement_readOrInitData()
{
	// read from rtc memory and test if data is valid
	system_rtc_mem_read(RTC_DATA_ADDRESS, &powermanagement_data, sizeof(powermanagement_data));
	if (powermanagement_data.magic != RTC_MAGIC)
	{
		// data not valid! create new data
		powermanagement_data.magic = RTC_MAGIC;
		powermanagement_data.lastMeasuredWaterLevel = LAST_MEASURED_WATER_LEVEL_INVALID;
		powermanagement_data.postUnchangedMeasurementCountDown = 0;
		powermanagement_data.shoudlEnterConfigurationMode = FALSE;
		powermanagement_data.shouldDoMeasurement = TRUE;
		powermanagement_data.shouldPostMeasurement = FALSE;
		powermanagement_data.shouldPostLog = FALSE;
		powermanagement_data.nextLogBytePointer = 0;
		os_printf("\nDeactivating modem ...\n");
		// save the data into RTC memory before we goto deep sleep
		log_save();
		system_rtc_mem_write(RTC_DATA_ADDRESS, &powermanagement_data, sizeof(powermanagement_data));
		system_deep_sleep_set_option(4);
		system_deep_sleep(DEEP_SLEEP_PERIOD_FOR_MODEM_ACTIVATION * 1000000);
		return FALSE;
	}
	return TRUE;
}

// delivers TRUE if the program should enter the configuration mode
unsigned char ICACHE_FLASH_ATTR powermanagement_shoudlEnterConfigurationMode()
{
	return powermanagement_data.shoudlEnterConfigurationMode;
}

// delivers TRUE if the program should do a water level measurement; and don't post the data to the internet
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldDoMeasurement()
{
	return powermanagement_data.shouldDoMeasurement;
}

// delivers TRUE if the program should post the measured data to the internet; and don't do a water level measurement
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldPostMeasurement()
{
	return powermanagement_data.shouldPostMeasurement;
}

// delivers TRUE TRUE if the program should post the log data to the internet; and don't do a water level measurement
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldPostLog()
{
	return powermanagement_data.shouldPostLog;
}

// checks the measurement
// pCurrentWaterLevel: the measured water level in mm
unsigned char ICACHE_FLASH_ATTR powermanagement_checkCurrentMeasurement(float pCurrentWaterLevel)
{
	// is the last data too old or does the measured water level differs too much?
	if (powermanagement_data.postUnchangedMeasurementCountDown == 0 ||
		fabs(powermanagement_data.lastMeasuredWaterLevel - pCurrentWaterLevel) >= (double)configuration_getMinDifferenceToPost())
	{
		// then save the current measurement
		powermanagement_data.lastMeasuredWaterLevel = pCurrentWaterLevel;
		// measurement should be posted
		powermanagement_data.shouldPostMeasurement = TRUE;
		powermanagement_data.shouldDoMeasurement = FALSE;
	}
	return powermanagement_data.shouldPostMeasurement;
}

// gets the measured water level in mm; that value that was saved in RTC memory
float ICACHE_FLASH_ATTR powermanagement_getLastMeasurement()
{
	return powermanagement_data.lastMeasuredWaterLevel;
}

// set the flags to signal that the measurement is posted successfully to the internet
void ICACHE_FLASH_ATTR powermanagement_measurementPosted()
{
	// posted now; => reset the countdown and the posting flag
	powermanagement_data.postUnchangedMeasurementCountDown = configuration_getMaxDataAgeToPost() / configuration_getDeepSleepPeriod();
	powermanagement_data.shouldPostMeasurement = FALSE;
	powermanagement_data.shouldDoMeasurement = TRUE;
}

// set the flags for measurement not posted => typ to post again after the next measurement
void ICACHE_FLASH_ATTR powermanagement_postingCanceled()
{
	// countdown is zero => after the next measurement the data will be posted!
	powermanagement_data.postUnchangedMeasurementCountDown = 0;
	powermanagement_data.shouldPostMeasurement = FALSE;
	powermanagement_data.shouldDoMeasurement = TRUE;
}

// goto deep sleep mode
void ICACHE_FLASH_ATTR powermanagement_deepSleep()
{
	// set the wakeup option and goto deep sleep mode
	unsigned int deepSleepPeriod = configuration_getDeepSleepPeriod() * 1000000;
	// wake up without modem
	unsigned char deepSleepOption = 4;
	if (powermanagement_data.shouldPostMeasurement == TRUE)
	{
		deepSleepPeriod = DEEP_SLEEP_PERIOD_FOR_MODEM_ACTIVATION * 1000000;
		// wake up with modem but don't calibrate RF
		deepSleepOption = 2;
		os_printf("\nActivating modem for posting data ...\n");
	}
	else if (powermanagement_data.shoudlEnterConfigurationMode == TRUE)
	{
		deepSleepPeriod = DEEP_SLEEP_PERIOD_FOR_MODEM_ACTIVATION * 1000000;
		// wake up with modem and calibrate RF
		deepSleepOption = 1;
		os_printf("\nActivating modem for configuration mode ...\n");
	}
	else
	{
		// decrement the countdown for posting the data
		if (powermanagement_data.postUnchangedMeasurementCountDown > 0)
		{
			powermanagement_data.postUnchangedMeasurementCountDown--;
		}
		os_printf("\nSleeping for %d seconds ...\n", configuration_getDeepSleepPeriod());
	}
	os_printf("Deep sleep option: %d\n", deepSleepOption);
	// save the data into RTC memory before we goto deep sleep
	log_save();
	system_rtc_mem_write(RTC_DATA_ADDRESS, &powermanagement_data, sizeof(powermanagement_data));
	// set wake-up option and start deep sleep
	system_deep_sleep_set_option(deepSleepOption);
	system_deep_sleep(deepSleepPeriod);
}

// preparation for entering the configuration mode
void ICACHE_FLASH_ATTR powermanagement_enterConfigurationMode()
{
	powermanagement_data.shoudlEnterConfigurationMode = TRUE;
	powermanagement_data.shouldPostMeasurement = FALSE;
	powermanagement_data.shouldDoMeasurement = FALSE;
	powermanagement_deepSleep();
}

// preparation for leaving the configuration mode
void ICACHE_FLASH_ATTR powermanagement_leaveConfigurationMode()
{
	powermanagement_data.shoudlEnterConfigurationMode = FALSE;
	powermanagement_data.shouldPostMeasurement = FALSE;
	powermanagement_data.shouldDoMeasurement = TRUE;
	os_printf("\nDeactivating modem ...\n");
	// save the data into RTC memory before we goto deep sleep
	log_save();
	system_rtc_mem_write(RTC_DATA_ADDRESS, &powermanagement_data, sizeof(powermanagement_data));
	system_deep_sleep_set_option(4);
	system_deep_sleep(DEEP_SLEEP_PERIOD_FOR_MODEM_ACTIVATION * 1000000);
}

// points to the next log byte; relative to the beginning of the log; starts with 0
unsigned int ICACHE_FLASH_ATTR powermanagement_getNextLogBytePointer()
{
	return powermanagement_data.nextLogBytePointer;
}
void ICACHE_FLASH_ATTR powermanagement_setNextLogBytePointer(unsigned int nextLogBytePointer)
{
	powermanagement_data.nextLogBytePointer = nextLogBytePointer;
}

// call this to signal that the program should post the log data to the internet; and don't do a water level measurement
void ICACHE_FLASH_ATTR powermanagement_setShouldPostLog(unsigned char shouldPostLog)
{
	powermanagement_data.shouldPostLog = shouldPostLog;
}