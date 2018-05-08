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

#ifndef __ultrasonicmeter_H__
#define __ultrasonicmeter_H__

typedef void ultrasonicMeter_finishedCallback();

// start the measurment process; that are MAX_MEASUREMENTS one shot ultrasonic measurement cycles
// with pIsSingleShotMode set to TRUE one single shot measurement can also be started
void ICACHE_FLASH_ATTR ultrasonicMeter_startMeasurement(ultrasonicMeter_finishedCallback *pFinished, unsigned char pIsSingleShotMode);
// gets the mean value of measured water level
float ICACHE_FLASH_ATTR ultrasonicMeter_getWaterLevel();
// gets the echo quality: 0 = no echo received; 10 = best possible; all 10 measurements are received
unsigned char ICACHE_FLASH_ATTR ultrasonicMeter_getEchoQuality();
// gets the distance that is measured in single shot mode
float ICACHE_FLASH_ATTR ultrasonicMeter_getSingleShotDistance();

#endif // __ultrasonicmeter_H__

