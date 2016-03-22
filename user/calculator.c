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

#define M_PI 3.14159265358979323846

#include "c_types.h"
#include "math.h"
#include <configuration.h>

// the last measured water level in millimeters
static float calculator_lastWaterLevel = 0.0;
// the last calculated wasser content in liters
static float calculator_liter = 0.0;
// the last calculated water level in centimeters
static float calculator_centimeter = 0.0;
// the last calculated water content in percent
static float calculator_percent = 0.0;

// function calculates new values from the given water level in millimeters
void ICACHE_FLASH_ATTR calculator_calculateNewValues(float waterLevel)
{
	float rMinusH;
	float radiusSquare;
	float diameter;
	unsigned char cisternType;
	unsigned int cisternRadius;
	unsigned int cisternLength;
	unsigned int distanceEmpty;
	unsigned int litersFull;

	// Water level unchanged?
	if (calculator_lastWaterLevel == waterLevel)
	{
		// nothing to do
		return;
	}

	// get the cistern parameters
	configuration_getCisternParameters(&cisternType, &cisternRadius, &cisternLength, &distanceEmpty, &litersFull);

	switch (cisternType)
	{
		// the cistern is a horizontal cylinder
	case 1:
		rMinusH = (float)cisternRadius - waterLevel;
		radiusSquare = ((float)cisternRadius * (float)cisternRadius);
		diameter = 2.0 * (float)cisternRadius;
		// calculate the liters. The cistern is aproximated by a horizontal cylinder
		calculator_liter = radiusSquare * cisternLength *
			(acosf(rMinusH / cisternRadius)
				- rMinusH * sqrtf(diameter * waterLevel - waterLevel * waterLevel) / radiusSquare)
			/ 1000000.0;
		break;

		// the cistern is a vertical cylinder
	case 2:
		radiusSquare = ((float)cisternRadius * (float)cisternRadius);
		// calculate the liters. The cistern is aproximated by a vertical cylinder
		calculator_liter = M_PI * radiusSquare * waterLevel / 1000000.0;
		break;

	default:
		calculator_liter = 0;
	}
			
	// The water level in centimeter is easy to calculate
	calculator_centimeter = waterLevel / 10.0;
	
	// calculate the content level in percent
	calculator_percent = calculator_liter / (float)litersFull * 100.0;
}  

// returns the last calculated wasser content in liters
float ICACHE_FLASH_ATTR calculator_getLiter()
{
	return calculator_liter;
}

// returns the last calculated water level in centimeters
float ICACHE_FLASH_ATTR calculator_getCentimeter()
{
	return calculator_centimeter;
}

// returns the last calculated water content in percent
float ICACHE_FLASH_ATTR calculator_getPercent()
{
	return calculator_percent;
}