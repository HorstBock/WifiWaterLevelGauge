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

#ifndef __log_H__
#define __log_H__

// enable the logging mechanism
void ICACHE_FLASH_ATTR log_enable(unsigned char logType);
// writes one character to the log buffer
void ICACHE_FLASH_ATTR log_write(char nextChar);
// save all buffers before going to deep sleep
void ICACHE_FLASH_ATTR log_save();
// posts the log
void ICACHE_FLASH_ATTR log_post();

#endif // __log_H__

