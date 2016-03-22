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

#ifndef __user_config_H__
#define __user_config_H__

// parameters for the access point that is acivated for listening for a configuration JSON
#define ACCESS_POINT_SSID "WaterLevelGauge"
#define ACCESS_POINT_PASSWORD "IoT-Wlg!4711#"
#define ACCESS_POINT_CHANNEL 8
#define ACCESS_POINT_LISTENING_PORT 1253

// URL for ThingSpeak
#define THINGSPEAK_URL "https://api.thingspeak.com/update?key=%s&field1=%d&field2=%d&field3=%d"

// turning the modem on or off works via a deep sleep cycle with 1 second
#define DEEP_SLEEP_PERIOD_FOR_MODEM_ACTIVATION 1

// version for the configuration data
#define CONFIGURATION_DATA_VERSION 1
// start sector in flash for configuration data (3 x 4KB blocks)
#define CONFIGURATION_DATA_START_SEC 0x75
// how many 4KB blocks of flash will be used for logging?
#define LOG_DATA_MAX_BLOCKS 4
// start sector in flash for the log
#define LOG_DATA_START_SEC 0x78

// Posting the data should not last longer than 60 seconds
#define POST_MEASUREMENT_TIMEOUT 60

// How long should the config button pressed at least before entering the configuration mode (2 seconds)
#define CONFIG_BUTTON_MIN_HOLD_DURATION 2

#endif // __user_config_H__