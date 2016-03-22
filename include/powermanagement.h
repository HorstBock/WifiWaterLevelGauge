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

#ifndef __powermanagement_H__
#define __powermanagement_H__

// delivers TRUE if the data was initialized and the module should go to one deep sleep cycle for disabling the modem
unsigned char ICACHE_FLASH_ATTR powermanagement_readOrInitData();
// delivers TRUE if the program should enter the configuration mode
unsigned char ICACHE_FLASH_ATTR powermanagement_shoudlEnterConfigurationMode();
// delivers TRUE if the program should do a water level measurement; and don't psot the data to the internet
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldDoMeasurement();
// delivers TRUE if the program should post the measured data to the internet; and don't do a water level measurement
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldPostMeasurement();
// delivers TRUE TRUE if the program should post the log data to the internet; and don't do a water level measurement
unsigned char ICACHE_FLASH_ATTR powermanagement_shouldPostLog();
// checks the measurement
// pCurrentWaterLevel: the measured water level in mm
unsigned char ICACHE_FLASH_ATTR powermanagement_checkCurrentMeasurement(float pCurrentWaterLevel);
// gets the measured water level in mm; that value that was saved in RTC memory
float ICACHE_FLASH_ATTR powermanagement_getLastMeasurement();
// set the flags to signal that the measurement is posted successfully to the internet
void ICACHE_FLASH_ATTR powermanagement_measurementPosted();
// set the flags for measurement not posted => typ to post again after the next measurement
void ICACHE_FLASH_ATTR powermanagement_postingCanceled();
// preparation for entering the configuration mode
void ICACHE_FLASH_ATTR powermanagement_enterConfigurationMode();
// preparation for leaving the configuration mode
void ICACHE_FLASH_ATTR powermanagement_leaveConfigurationMode();
// goto deep sleep mode
void ICACHE_FLASH_ATTR powermanagement_deepSleep();
// points to the next log byte; relative to the beginning of the log; starts with 0
unsigned int ICACHE_FLASH_ATTR powermanagement_getNextLogBytePointer();
void ICACHE_FLASH_ATTR powermanagement_setNextLogBytePointer(unsigned int nextLogBytePointer);
// call this to signal that the program should post the log data to the internet; and don't do a water level measurement
void ICACHE_FLASH_ATTR powermanagement_setShouldPostLog(unsigned char shouldPostLog);

#endif // __powermanagement_H__
