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
#include "mem.h"
#include "espconn.h"
#include <espmissingincludes.h>
#include <powermanagement.h>
#include <configuration.h>

// posting the log to the host is done in 512 byte chunks
#define POST_CHUNK_SIZE 512

// if TRUE the logging mechanism is enabled
static unsigned char log_type = 0;
// pointer to a 4KB block as a buffer for the log that should be written to flash
static char* log_buffer = NULL;
// for equal flash cell degeneration we start writing to the flash not at the first block
static unsigned char log_rollingStartBlock = 0;
// log block counter
static unsigned char log_currentBlockNumber = 0;
// next byte in the flash
static unsigned short log_nextLogByteInBlock = 0;
// if set to TRUE the log is full
static unsigned char log_isFull = FALSE;
// the socket for sending the log data
static struct espconn log_socketConnection;
// the tcp connection for sending log data
static esp_tcp log_tcpConnection;
// send block counter
static unsigned char log_postBlockNumber = 0;
// next byte in block zo send
static unsigned short log_postByteInBlock = 0;
// buffer for the next chunk to post
static char* log_chunkToPostBuffer = NULL;
// true if the last chunk of the log was posted
static char log_lastChunkPosted = FALSE;

// for equal flash cell degeneration we start writing to the flash not at the first block
// gets the real block number in the flash memory
static unsigned char ICACHE_FLASH_ATTR log_getRealFlashBlockNumber()
{
	unsigned short blockNumber = log_rollingStartBlock + log_currentBlockNumber;
	if (blockNumber >= LOG_DATA_MAX_BLOCKS)
	{
		blockNumber = log_currentBlockNumber - log_rollingStartBlock;
	}
	return (unsigned char)(LOG_DATA_START_SEC + blockNumber);
}

// enable the logging mechanism
void ICACHE_FLASH_ATTR log_enable(unsigned char logType)
{
	// init the log pointers
	unsigned int nextLogBytePointer = powermanagement_getNextLogBytePointer();
	log_rollingStartBlock = (unsigned char)(nextLogBytePointer >> 24);
	log_currentBlockNumber = (unsigned char)((nextLogBytePointer & 0x00FFFFFF) / SPI_FLASH_SEC_SIZE);
	log_nextLogByteInBlock = (unsigned short)((nextLogBytePointer & 0x00FFFFFF) % SPI_FLASH_SEC_SIZE);
	if (log_currentBlockNumber >= LOG_DATA_MAX_BLOCKS)
	{
		log_isFull = TRUE;
		return;
	}
	// enable by setting the log type
	log_type = logType;
	if (logType != 0)
	{
		// allocate memory and read the current log block into the buffer
		log_buffer = (char*)os_malloc(SPI_FLASH_SEC_SIZE);
		spi_flash_read(log_getRealFlashBlockNumber() * SPI_FLASH_SEC_SIZE, (uint32*)log_buffer, SPI_FLASH_SEC_SIZE);
	}
}

// writes one character to the log buffer
void ICACHE_FLASH_ATTR log_write(char nextChar)
{
	// logging disabled or log full
	if (log_type == 0 || log_isFull == TRUE)
	{
		return;
	}

	// store the character in the buffer
	log_buffer[log_nextLogByteInBlock] = nextChar;
	log_nextLogByteInBlock++;

	// buffer full?
	if (log_nextLogByteInBlock == SPI_FLASH_SEC_SIZE)
	{
		// erase the block and write the buffer to the flash
		unsigned char blockNumber = log_getRealFlashBlockNumber();
		spi_flash_erase_sector(blockNumber);
		spi_flash_write(blockNumber * SPI_FLASH_SEC_SIZE, (uint32*)log_buffer, SPI_FLASH_SEC_SIZE);
		// reset / increment the pointer clear the buffer and test if log is full
		log_nextLogByteInBlock = 0;
		log_currentBlockNumber++;
		os_memset(log_buffer, 0, SPI_FLASH_SEC_SIZE);
		if (log_currentBlockNumber >= LOG_DATA_MAX_BLOCKS)
		{
			log_isFull = TRUE;
		}
	}
}

// save all buffers before going to deep sleep
void ICACHE_FLASH_ATTR log_save()
{
	// logging disabled?
	if (log_type == 0)
	{
		return;
	}
	// erase the block and write the buffer to the flash
	unsigned char blockNumber = log_getRealFlashBlockNumber();
	// skip if the block is empty
	if (log_nextLogByteInBlock > 0)
	{
		spi_flash_erase_sector(blockNumber);
		spi_flash_write(blockNumber * SPI_FLASH_SEC_SIZE, (uint32*)log_buffer, SPI_FLASH_SEC_SIZE);
	}
	// store the pointer to the next log byte in the RTC data area that deep sleep will survive
	unsigned int next = log_currentBlockNumber * SPI_FLASH_SEC_SIZE + log_nextLogByteInBlock;
	powermanagement_setNextLogBytePointer((log_rollingStartBlock << 24) + next);
	
	// is the log full or below of 10% free?
	if (log_isFull || LOG_DATA_MAX_BLOCKS * SPI_FLASH_SEC_SIZE - next < LOG_DATA_MAX_BLOCKS * SPI_FLASH_SEC_SIZE / 10)
	{
		powermanagement_setShouldPostLog(TRUE);
	}
	// free the allocated memory
	os_free(log_buffer);
}

// will be called after the posting of the log is done
static void ICACHE_FLASH_ATTR log_postDone()
{
	// posting is done!
	powermanagement_setShouldPostLog(FALSE);
	// adjust the start block
	log_rollingStartBlock++;
	if (log_rollingStartBlock == LOG_DATA_MAX_BLOCKS)
	{
		log_rollingStartBlock = 0;
	}
	// reset the buffer and the pointers
	log_currentBlockNumber = 0;
	log_nextLogByteInBlock = 0;
	os_memset(log_buffer, 0, SPI_FLASH_SEC_SIZE);

	// free memory and disonnect from the TCP server
	os_free(log_chunkToPostBuffer);
	if (log_type == 2)
	{
		espconn_secure_disconnect(&log_socketConnection);
	}
	else
	{
		espconn_disconnect(&log_socketConnection);
	}
}

// callback will be called after one chuck of data was sent
static void ICACHE_FLASH_ATTR log_sentCallback(void * arg)
{
	// last chunk posted? => then done
	if (log_lastChunkPosted == TRUE)
	{
		log_postDone();
		return;
	}

	// from which flash block should the next chunk be read
	unsigned short blockNumber = log_rollingStartBlock + log_postBlockNumber;
	if (blockNumber >= LOG_DATA_MAX_BLOCKS)
	{
		blockNumber = log_postBlockNumber - log_rollingStartBlock;
	}
	blockNumber = LOG_DATA_START_SEC + blockNumber;
	// read the next chunk from flash
	spi_flash_read(blockNumber * SPI_FLASH_SEC_SIZE + log_postByteInBlock, (uint32*)log_chunkToPostBuffer, POST_CHUNK_SIZE);

	// last block and last chunk?
	unsigned short bytesToSend = POST_CHUNK_SIZE;
	if (blockNumber == log_getRealFlashBlockNumber() && log_nextLogByteInBlock - log_postByteInBlock <= POST_CHUNK_SIZE)
	{
		// reduced bytes to send count for last chunk
		bytesToSend = log_nextLogByteInBlock - log_postByteInBlock;
		log_lastChunkPosted = TRUE;
	}

	// increment the pointers
	log_postByteInBlock += POST_CHUNK_SIZE;
	if (log_postByteInBlock == SPI_FLASH_SEC_SIZE)
	{
		log_postByteInBlock = 0;
		log_postBlockNumber++;
	}

	// send the next chunk
	if (log_type == 2)
	{
		espconn_secure_sent(&log_socketConnection, (uint8_t *)log_chunkToPostBuffer, bytesToSend);
	}
	else
	{
		espconn_sent(&log_socketConnection, (uint8_t *)log_chunkToPostBuffer, bytesToSend);
	}
}

// will be called after the connection to the TCP server was established
static void ICACHE_FLASH_ATTR log_connectCallback(void * args)
{
	struct espconn * conn = (struct espconn *)args;
	espconn_regist_sentcb(&log_socketConnection, log_sentCallback);
	// send first chunk
	log_sentCallback(NULL);
}

// will be called if disconnected from the TCP server
static void ICACHE_FLASH_ATTR log_disconnectCallback(void * arg)
{
	espconn_delete(&log_socketConnection);
	if (log_chunkToPostBuffer != NULL)
	{
		os_free(log_chunkToPostBuffer);
		log_chunkToPostBuffer = NULL;
	}
}

// will be called after an error has occured
static void ICACHE_FLASH_ATTR log_errorCallback(void *arg, sint8 errType)
{
	os_printf("Disconnected with error %d\n", errType);
	log_disconnectCallback(arg);
}

// will be called after the DNS query has finished
static void ICACHE_FLASH_ATTR log_dnsCallback(const char* hostname, ip_addr_t* ipAddress, void* args)
{
	if (ipAddress == NULL)
	{
		os_printf("DNS failed for %s\n", hostname);
	}
	else
	{
		// initialize the connection structure
		log_socketConnection.type = ESPCONN_TCP;
		log_socketConnection.state = ESPCONN_NONE;
		os_memcpy(log_tcpConnection.remote_ip, ipAddress, 4);
		log_tcpConnection.local_port = espconn_port();
		log_tcpConnection.remote_port = configuration_getLogPort();
		log_socketConnection.proto.tcp = &log_tcpConnection;
		espconn_regist_connectcb(&log_socketConnection, log_connectCallback);
		espconn_regist_disconcb(&log_socketConnection, log_disconnectCallback);
		espconn_regist_reconcb(&log_socketConnection, log_errorCallback);

		// connect to the TCP server
		if (log_type == 2)
		{
			espconn_secure_set_size(ESPCONN_CLIENT, 5120); // set SSL buffer size
			espconn_secure_connect(&log_socketConnection);
		}
		else
		{
			espconn_connect(&log_socketConnection);
		}
	}
}
// posts the log
void ICACHE_FLASH_ATTR log_post()
{
	// logging disabled?
	if (log_type == 0)
	{
		return;
	}

	// TODO: I would be better to read the last data directly from the buffer; as workaround I save it to the flash
	// erase the block and write the buffer to the flash
	unsigned char blockNumber = log_getRealFlashBlockNumber();
	// skip if the block is empty
	if (log_nextLogByteInBlock > 0)
	{
		spi_flash_erase_sector(blockNumber);
		spi_flash_write(blockNumber * SPI_FLASH_SEC_SIZE, (uint32*)log_buffer, SPI_FLASH_SEC_SIZE);
	}

	// initialize the pointers
	log_postBlockNumber = 0;
	log_postByteInBlock = 0;
	log_lastChunkPosted = FALSE;
	log_chunkToPostBuffer = (char*)os_malloc(SPI_FLASH_SEC_SIZE);
	ip_addr_t ipAddress;
	char* logHost = configuration_getLogHost();

	// start the DNS query
	err_t error = espconn_gethostbyname(NULL, logHost, &ipAddress, log_dnsCallback);
	if (error == ESPCONN_INPROGRESS)
	{
		// OK; DNS query is in progress
		return;
	}
	else if (error == ESPCONN_OK)
	{
		// Already in the local names table (or hostname was an IP address), execute the callback ourselves.
		log_dnsCallback(logHost, &ipAddress, NULL);
	}
	else
	{
		if (error == ESPCONN_ARG)
		{
			os_printf("DNS arg error %s\n", logHost);
		}
		else
		{
			os_printf("DNS error code %d\n", error);
		}
		log_dnsCallback(logHost, NULL, NULL); // Handle all DNS errors the same way.
	}
}