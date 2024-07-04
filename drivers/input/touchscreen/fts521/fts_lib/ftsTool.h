/**
  ******************************************************************************
  * @file    ftsTool.h
  * @brief   Contains all the definitions to support common operations inside the
  * driver
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

#ifndef FTS_TOOL_H
#define FTS_TOOL_H

char *printHex(char *label, u8 *buff, int count, u8 *result);
int u8ToU16(u8 *src, u16 *dst);
int u8ToU16_be(u8 *src, u16 *dst);
int u8ToU16n(u8 *src, int src_length, u16 *dst);
int u16ToU8(u16 src, u8 *dst);
int u16ToU8_be(u16 src, u8 *dst);
int u16ToU8n_be(u16 *src, int src_length, u8 *dst);
int u8ToU32(u8 *src, u32 *dst);
int u8ToU32_be(u8 *src, u32 *dst);
int u32ToU8(u32 src, u8 *dst);
int u32ToU8_be(u32 src, u8 *dst);
int u8ToU64_be(u8 *src, u64 *dest, int size);
int u64ToU8_be(u64 src, u8 *dest, int size);
int attempt_function(int (*code)(void), unsigned long wait_before_retry,
		     int retry_count);
int senseOn(void);
int senseOff(void);
void print_frame_short(char *label, short **matrix, int row, int column);
short **array1dTo2d_short(short *data, int size, int columns);
void print_frame_u16(char *label, u16 **matrix, int row, int column);
u16 **array1dTo2d_u16(u16 *data, int size, int columns);
u8 **array1dTo2d_u8(u8 *data, int size, int columns);
i8 **array1dTo2d_i8(i8 *data, int size, int columns);
void print_frame_u8(char *label, u8 **matrix, int row, int column);
void print_frame_i8(char *label, i8 **matrix, int row, int column);
void print_frame_u32(char *label, u32 **matrix, int row, int column);
void print_frame_int(char *label, int **matrix, int row, int column);
int cleanUp(int enableTouch);
int flushFIFO(void);

/* New API */
int fromIDtoMask(u8 id, u8 *mask, int size);

#endif
