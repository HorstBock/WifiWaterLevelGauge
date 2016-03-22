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

#ifndef IO_H
#define IO_H

// The config button is connected to GPIO0
#define CONFIG_BUTTON_GPIO 0
// The ultrasonic trigger pin is connected to GPIO12
#define TRIGGER_GPIO 12
// The ultrasonic echo input is connected to GPIO13
#define ECHO_GPIO 13
// The general purpose LED is attached to GPIO14
#define LED_GPIO 14

// initalize the hardware
void ICACHE_FLASH_ATTR io_init();
// function starts the configuration button be observation
void ICACHE_FLASH_ATTR io_startConfigButtonObservation();
// function turns the led on or off
void ICACHE_FLASH_ATTR io_ledSet(unsigned char state);
// function starts pulsing the led on for one time
void ICACHE_FLASH_ATTR io_ledPulse(unsigned short pulsePeriodInMs);
// function start the led blink mode
void ICACHE_FLASH_ATTR io_ledBlink(unsigned short onPeriodInMs, unsigned short offPeriodInMs);

#endif