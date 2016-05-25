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
#include <espmissingincludes.h>
#include <io.h>
#include <stdout.h>
#include <ultrasonicmeter.h>
#include <powermanagement.h>
#include <configuration.h>
#include <posting.h>
#include <log.h>

// Version number
#define WIFI_WATER_LEVEL_GAUGE_VERSION "1.2"

// The main entry point.
void user_init(void)
{
	// initialize uart and input/outpute
	stdout_init();
	io_init();
	
	os_printf("\n\nWifi water level gauge version: %s\n", WIFI_WATER_LEVEL_GAUGE_VERSION);
	os_printf("Build with Espressif NONOS-SDK version: %s\n", system_get_sdk_version());

	// no data in RTC memory found => first start
	if (powermanagement_readOrInitData() == FALSE)
	{
		os_printf("\nInitial start ...\n");
		return;
	}

	// read the configuration from flash
	unsigned char configurationFound = FALSE;
	if (powermanagement_shoudlEnterConfigurationMode() == FALSE)
	{
		configurationFound = configuration_init();
	}
	// enable logging?
	if (configurationFound == TRUE)
	{
		unsigned char logType = configuration_getLogType();
		if (logType != 0)
		{
			log_enable(logType);
		}
	}

	// the config button should be observed from now on
	io_startConfigButtonObservation();

	// should we enter the configuration mode?
	if (powermanagement_shoudlEnterConfigurationMode() == TRUE)
	{
		// let the led blink slowly and start the configuration
		io_ledBlink(500, 500);
		configuration_start();
	}
	// no configuration data found?
	else if (configurationFound == FALSE)
	{
		return;
	}
	// should we do a ultrasonic measurement
	else if (powermanagement_shouldDoMeasurement() == TRUE)
	{
		wifi_set_opmode_current(NULL_MODE);
		ultrasonicMeter_startMeasurement(posting_checkIfPostNeeded);
	}
	// should we post the measured data?
	else if (powermanagement_shouldPostMeasurement() == TRUE || powermanagement_shouldPostLog() == TRUE)
	{
		os_printf("\nStarting sending data ...\n");
		io_ledSet(1);
		wifi_set_event_handler_cb(posting_start);
		wifi_set_opmode_current(STATION_MODE);
		wifi_station_connect();

		// enable the posting timeout timer
		posting_startTimeoutTimer();
	}
	else
	{
		os_printf("\n!!! Configuration error !!!\n");
	}
}