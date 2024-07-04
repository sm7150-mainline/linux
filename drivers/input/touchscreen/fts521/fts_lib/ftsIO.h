/**
  ******************************************************************************
  * @file    ftsIO.h
  * @brief   Contains all the definitions and prototypes used and implemented in
  * ftsIO.c
  ******************************************************************************
  * Copyright (C) 2016-2023, STMicroelectronics - All Rights Reserved
  * Author(s): ACD(Analog Custom Devices) Software Team for STMicroelectronics.
  *
  * License terms: GPL V2.0.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  *
  * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
  * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
  * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
  * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
  * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM
  * THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
  * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
  *
  * You should have received a copy of the GNU General Public License along with
  * this program. If not, see <http://www.gnu.org/licenses/>.
  *
  ******************************************************************************
  */

#ifndef FTS_IO_H
#define FTS_IO_H

#include "ftsSoftware.h"

#define I2C_RETRY \
	3 /* /< number of retry in case of i2c
					 * failure */
#define I2C_WAIT_BEFORE_RETRY \
	2 /* /< wait in ms before retry an i2c
					 * transaction */

#ifdef I2C_INTERFACE
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
struct i2c_client *getClient(void);
#else
#include <linux/spi/spi.h>
struct spi_device *getClient(void);
#endif

int openChannel(void *clt);
struct device *getDev(void);

/*************** NEW I2C API ****************/
#ifdef I2C_INTERFACE
int changeSAD(u8 sad);
#endif
int fts_read(u8 *outBuf, int byteToRead);
int fts_writeRead(u8 *cmd, int cmdLength, u8 *outBuf, int byteToRead);
int fts_write(u8 *cmd, int cmdLength);
int fts_writeFwCmd(u8 *cmd, int cmdLenght);
int fts_writeThenWriteRead(u8 *writeCmd1, int writeCmdLength, u8 *readCmd1,
			   int readCmdLength, u8 *outBuf, int byteToRead);
int fts_writeU8UX(u8 cmd, AddrSize addrSize, u64 address, u8 *data,
		  int dataSize);
int fts_writeReadU8UX(u8 cmd, AddrSize addrSize, u64 address, u8 *outBuf,
		      int byteToRead, int hasDummyByte);
int fts_writeU8UXthenWriteU8UX(u8 cmd1, AddrSize addrSize1, u8 cmd2,
			       AddrSize addrSize2, u64 address, u8 *data,
			       int dataSize);
int fts_writeU8UXthenWriteReadU8UX(u8 cmd1, AddrSize addrSize1, u8 cmd2,
				   AddrSize addrSize2, u64 address, u8 *outBuf,
				   int count, int hasDummyByte);
#endif
