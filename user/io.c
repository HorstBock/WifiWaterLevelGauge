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

#include "c_types.h"
#include "user_interface.h"
#include "osapi.h"
#include "gpio.h"
#include "smartconfig.h"
#include <espmissingincludes.h>
#include <powermanagement.h>
#include <io.h>

// timer structure for observing the configuration button
static ETSTimer io_buttonTimer;
// counter for counting how long the configuration button is pressed
static int io_buttonCounter = 0;
// timer structure for the led pulse and blink operations
static ETSTimer io_ledTimer;
// if the led should blink that's the on period in milliseconds
static unsigned short io_ledOnPeriodInMs;
// if the led should blink that's the off period in milliseconds
static unsigned short io_ledOffPeriodInMs;
// the last led state (0 = led off; 1 = led on)
static unsigned char io_ledState;

// timer callback for observing the button
static void ICACHE_FLASH_ATTR io_buttonTimerTick(void *arg)
{
	// is config button pressed?
	if (!GPIO_INPUT_GET(CONFIG_BUTTON_GPIO))
	{
		// then count
		io_buttonCounter++;
	}
	else // the config button is released!
	{
		// was the button pressed for at least 2 seconds?
		if (io_buttonCounter >= CONFIG_BUTTON_MIN_HOLD_DURATION * 1000 / 500)
		{
			os_printf("\nConfiguration button pressed ...\n");
			powermanagement_enterConfigurationMode();
		}
		io_buttonCounter = 0;
	}
}

// timer callback for pulsing the led
static void ICACHE_FLASH_ATTR io_ledPulseTimerTick(void *arg)
{
	// pulse is over; disable the timer and turn the led off
	os_timer_disarm(&io_ledTimer);
	gpio_output_set(0, (1 << LED_GPIO), (1 << LED_GPIO), 0);
}

// timer callback for blinking with the led
static void ICACHE_FLASH_ATTR io_ledBlinkTimerTick(void *arg)
{
	// disable the timer and turn the led on or off
	io_ledSet(io_ledState);
	// activate the timer again
	os_timer_setfn(&io_ledTimer, io_ledBlinkTimerTick, NULL);
	os_timer_arm(&io_ledTimer, (int)(io_ledState == 0 ? io_ledOffPeriodInMs : io_ledOnPeriodInMs), 0);
	// alternate the state
	io_ledState = 1 - io_ledState;
}

// initalize the hardware
void ICACHE_FLASH_ATTR io_init()
{
	// set pin functions; they should work as general purpose inputs/outputs (GPIO's)
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);

	// TRIGGER_GPIO and WIFI_LED_GPIO are the outputs and CONFIG_BUTTON_GPIO and ECHO_GPIO are the inputs
	gpio_output_set(0, 0, (1 << TRIGGER_GPIO) | (1 << LED_GPIO), (1 << CONFIG_BUTTON_GPIO) | (1 << ECHO_GPIO));
	// don't trigger the ultrasonic sensor and switch the wifi led off
	gpio_output_set(0, (1 << TRIGGER_GPIO) | (1 << LED_GPIO), (1 << TRIGGER_GPIO) | (1 << LED_GPIO), 0);
}

// function starts the configuration button be observation
void ICACHE_FLASH_ATTR io_startConfigButtonObservation()
{
	// enable the timer for the config button
	os_timer_disarm(&io_buttonTimer);
	os_timer_setfn(&io_buttonTimer, io_buttonTimerTick, NULL);
	os_timer_arm(&io_buttonTimer, 500, 1);
}

// function turns the led on or off
void ICACHE_FLASH_ATTR io_ledSet(unsigned char state)
{
	// disable timer (maybe we are in timer mode)
	os_timer_disarm(&io_ledTimer);
	if (state == 0)
	{
		// turn the led off
		gpio_output_set(0, (1 << LED_GPIO), (1 << LED_GPIO), 0);
	}
	else
	{
		// turn the led on
		gpio_output_set((1 << LED_GPIO), 0, (1 << LED_GPIO), 0);
	}
}

// function starts pulsing the led on for one time
void ICACHE_FLASH_ATTR io_ledPulse(unsigned short pulsePeriodInMs)
{
	io_ledSet(1);
	// activate the timer
	os_timer_setfn(&io_ledTimer, io_ledPulseTimerTick, NULL);
	os_timer_arm(&io_ledTimer, (int)pulsePeriodInMs, 0);
}

// function start the led blink mode
void ICACHE_FLASH_ATTR io_ledBlink(unsigned short onPeriodInMs, unsigned short offPeriodInMs)
{
	io_ledOnPeriodInMs = onPeriodInMs;
	io_ledOffPeriodInMs = offPeriodInMs;
	io_ledState = 0;
	io_ledSet(1);
	// activate the timer
	os_timer_setfn(&io_ledTimer, io_ledBlinkTimerTick, NULL);
	os_timer_arm(&io_ledTimer, (int)onPeriodInMs, 0);
}