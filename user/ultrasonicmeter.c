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
#include "gpio.h"
#include <espmissingincludes.h>
#include <io.h>
#include <configuration.h>
#include <ultrasonicmeter.h>

// modes for the state machine
// initial state
#define WAITFOR_NOTHING 0
// waiting for positive edge on echo pin
#define WAITFOR_ECHO_POSITIVE_EDGE 1
// waiting for negative edge on echo pin
#define WAITFOR_ECHO_NEGATIVE_EDGE 2
// waiting for ultrasonic silence
#define WAITFOR_SILENCE 3
// we are done; measurement is finished
#define FINISHED 4

// Duration for waiting for "silence" (ultrasonic silence) in milliseconds, that's the timespan between two single shot measurements
#define SILENCE_TIMESPAN_MS 1500

// Unit of measurement according to the datashett of HC-SR04 (58 µs / cm = 5,8 µs / mm)
#define US_PER_MM 5.8

// Length of the trigger pulse in µs
#define TRIGGER_PULSE_US 15

// how often should the module do a measurement?
#define MAX_MEASUREMENTS 10

// Echo quality 0 = no echo received; MAX_MEASUREMENTS = best possible; all MAX_MEASUREMENTS measurements are received
static unsigned char ultrasonicMeter_valueQuality = 5;
// all the measured values
static float ultrasonicMeter_measuredDistances[MAX_MEASUREMENTS];
// the index in the ultrasonicMeter_MeasuredDistances array
static unsigned char ultrasonicMeter_measuredDistancesIndex = 0;
// current state
static unsigned char ultrasonicMeter_currentState = WAITFOR_NOTHING;
// Echo start timestamp
static uint32 ultrasonicMeter_startTime;
// Echo stop timestamp
static uint32 ultrasonicMeter_stopTime;

// the timer for stating a new cycle
static ETSTimer ultrasonicMeter_triggerNewCycleTimer;

// call this function after all measurements are done
static ultrasonicMeter_finishedCallback *ultrasonicMeter_finished = NULL;

static unsigned char ultrasonicMeter_isSingleShotMode = FALSE;

// gets the echo quality: 0 = no echo received; MAX_MEASUREMENTS = best possible; all MAX_MEASUREMENTS measurements are received
unsigned char ICACHE_FLASH_ATTR ultrasonicMeter_getEchoQuality()
{
	return ultrasonicMeter_valueQuality;
}

// gets the distance that is measured in single shot mode
float ICACHE_FLASH_ATTR ultrasonicMeter_getSingleShotDistance()
{
	return ultrasonicMeter_measuredDistances[0];
}

// gets the mean value of measured water level
float ICACHE_FLASH_ATTR ultrasonicMeter_getWaterLevel()
{
	int valueCount = 0;
	float sum = 0;
	float min = 10000.0;
	float max = 0.0;
	for (int i = 0; i < MAX_MEASUREMENTS; i++)
	{
		if (ultrasonicMeter_measuredDistances[i] > 0)
		{
			sum += ultrasonicMeter_measuredDistances[i];
			if (ultrasonicMeter_measuredDistances[i] < min)
			{
				min = ultrasonicMeter_measuredDistances[i];
			}
			if (ultrasonicMeter_measuredDistances[i] > max)
			{
				max = ultrasonicMeter_measuredDistances[i];
			}
			valueCount++;
		}
	}
	// do we have at least 3 values?
	if (valueCount >= 3)
	{
		// then cut the min an max value
		sum = sum - max - min;
		valueCount -= 2;
	}
	return (valueCount > 0) ? (float)configuration_getDistanceEmpty() - sum / valueCount : 0.0;
}

// interrupt handler
// this function will be executed on any edge of ECHO_GPIO
static void ICACHE_FLASH_ATTR ultrasonicMeter_gpioEvent(void *args)
{
	// read interrupt status
	uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	// if the interrupt was by ECHO_GPIO
	if (gpio_status & BIT(ECHO_GPIO))
	{
		// clear interrupt status
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(ECHO_GPIO));

		// are we waiting for the positive edge?
		if (ultrasonicMeter_currentState == WAITFOR_ECHO_POSITIVE_EDGE)
		{
			// wait for the negative edge
			gpio_pin_intr_state_set(GPIO_ID_PIN(ECHO_GPIO), GPIO_PIN_INTR_NEGEDGE);

			// set start timestamp and set the next state 
			ultrasonicMeter_startTime = system_get_time();
			ultrasonicMeter_currentState = WAITFOR_ECHO_NEGATIVE_EDGE;
		}
		// are we waiting for the negative edge?
		else if (ultrasonicMeter_currentState == WAITFOR_ECHO_NEGATIVE_EDGE)
		{
			// disable interupt
			gpio_pin_intr_state_set(GPIO_ID_PIN(ECHO_GPIO), GPIO_PIN_INTR_DISABLE);

			// set stop timestamp and set the next state 
			ultrasonicMeter_stopTime = system_get_time();
			ultrasonicMeter_currentState = WAITFOR_SILENCE;

			// calculate distance
			float distance = ((float)ultrasonicMeter_stopTime - (float)ultrasonicMeter_startTime) / (float)US_PER_MM;
			// distance wider than expected?
			if (distance > (float)configuration_getDistanceEmpty())
			{
				distance = -1;
			}
			// store the measured value
			ultrasonicMeter_measuredDistances[ultrasonicMeter_measuredDistancesIndex] = distance;
			ultrasonicMeter_measuredDistancesIndex++;
			os_printf("Distance = %d mm\n", (int)distance);

			// if the distance is negative we have a fault during receiving the ultrasonic echo
			if (distance > 0.0)
			{
				// Increment the echo receiving quality
				if (ultrasonicMeter_valueQuality < MAX_MEASUREMENTS)
				{
					ultrasonicMeter_valueQuality++;
				}
			}
			// Measurement fault!
			else
			{
				// Increment the echo receiving quality
				if (ultrasonicMeter_valueQuality > 0)
				{
					ultrasonicMeter_valueQuality--;
				}
			}
		}
	}

	// do we have finished (array for measured values full) or single shot mode?
	if (ultrasonicMeter_measuredDistancesIndex == MAX_MEASUREMENTS ||
		(ultrasonicMeter_measuredDistancesIndex == 1 && ultrasonicMeter_isSingleShotMode == TRUE))
	{
		// then stop
		// Disable interrupts by GPIO and disarm the timer
		ETS_GPIO_INTR_DISABLE();
		os_timer_disarm(&ultrasonicMeter_triggerNewCycleTimer);
		// set the state
		ultrasonicMeter_currentState = FINISHED;
		// callback
		if (ultrasonicMeter_finished != NULL)
		{
			ultrasonicMeter_finished();
		}
	}
}

// triggers a new ultrasonic measurement cycle
static void ICACHE_FLASH_ATTR ultrasonicMeter_triggerNewCycle(void *arg)
{
	if (ultrasonicMeter_measuredDistancesIndex == 0)
	{
		os_printf("Starting range measurement...\n");
		os_timer_disarm(&ultrasonicMeter_triggerNewCycleTimer);
		os_timer_setfn(&ultrasonicMeter_triggerNewCycleTimer, ultrasonicMeter_triggerNewCycle, NULL);
		os_timer_arm(&ultrasonicMeter_triggerNewCycleTimer, SILENCE_TIMESPAN_MS, 1);
	}
	// test the current state
	if (ultrasonicMeter_currentState != WAITFOR_SILENCE && ultrasonicMeter_currentState != WAITFOR_NOTHING)
	{
		// Empfangsqualität verschlechtern!
		if (ultrasonicMeter_valueQuality > 0)
		{
			ultrasonicMeter_valueQuality--;
		}
	}

	if (GPIO_INPUT_GET(ECHO_GPIO))
	{
		os_printf("Echo pin high! Next measurement not possible!\n");
		return;
	}

	io_ledPulse(100);
	ultrasonicMeter_currentState = WAITFOR_ECHO_POSITIVE_EDGE;
	gpio_output_set((1 << TRIGGER_GPIO), 0, (1 << TRIGGER_GPIO), 0);
	os_delay_us(TRIGGER_PULSE_US);
	gpio_output_set(0, (1 << TRIGGER_GPIO), (1 << TRIGGER_GPIO), 0);
	// trigger interrupt on positive edge on echo pin
	gpio_pin_intr_state_set(GPIO_ID_PIN(ECHO_GPIO), GPIO_PIN_INTR_POSEDGE);
}

// start the measurment process; that are MAX_MEASUREMENTS one shot ultrasonic measurement cycles
// with pIsSingleShotMode set to TRUE one single shot measurement can also be started
void ICACHE_FLASH_ATTR ultrasonicMeter_startMeasurement(ultrasonicMeter_finishedCallback *pFinished, unsigned char pIsSingleShotMode)
{
	ultrasonicMeter_finished = pFinished;
	ultrasonicMeter_isSingleShotMode = pIsSingleShotMode;

	// Attach interrupt handle to gpio interrupts.
	ETS_GPIO_INTR_ATTACH(ultrasonicMeter_gpioEvent, NULL);
	// Disable interrupts by GPIO
	ETS_GPIO_INTR_DISABLE();
	// not sure why I should call this but found it in several examples
	gpio_register_set(GPIO_PIN_ADDR(ECHO_GPIO), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
		| GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
		| GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
	// Clear interrupt status
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(ECHO_GPIO));
	// Enable interrupts by GPIO
	ETS_GPIO_INTR_ENABLE();
	
	ultrasonicMeter_measuredDistancesIndex = 0;
	ultrasonicMeter_triggerNewCycle(NULL);
}