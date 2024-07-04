/**
  ******************************************************************************
  * @file    ftsTime.h
  * @brief   Contains all the definitions and structs to handle and measure the
  * time in the driver
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

#ifndef FTS_TIME_H
#define FTS_TIME_H

#include <linux/time.h>

/* TIMEOUT */
/** @defgroup timeouts	 Timeouts
  * Definitions of all the Timeout used in several operations
  * @{
  */
#define TIMEOUT_RESOLUTION 50
/* /< timeout resolution in ms (all timeout should be multiples of this unit) */
#define GENERAL_TIMEOUT (15 * TIMEOUT_RESOLUTION)
/* /< general timeout in ms */
#define RELEASE_INFO_TIMEOUT (2 * TIMEOUT_RESOLUTION)
/* /< timeout to request release info in ms */

#define TIMEOUT_REQU_COMP_DATA (4 * TIMEOUT_RESOLUTION)
/* /< timeout to request compensation data in ms */
#define TIMEOUT_REQU_DATA (8 * TIMEOUT_RESOLUTION)
/* /< timeout to request data in ms */
#define TIMEOUT_ITO_TEST_RESULT (4 * TIMEOUT_RESOLUTION)
/* /< timeout to perform ito test in ms */
#define TIMEOUT_INITIALIZATION_TEST_RESULT (5000 * TIMEOUT_RESOLUTION)
/* /< timeout to perform initialization test in ms */
#define TIEMOUT_ECHO (50 * TIMEOUT_RESOLUTION)
/* /< timeout of the echo command,*/
#define TIMEOUT_ECHO_FPI (200 * TIMEOUT_RESOLUTION)
/* /< timeout of the Full panel Init echo command */
#define TIMEOUT_ECHO_SINGLE_ENDED_SPECIAL_AUTOTUNE (100 * TIMEOUT_RESOLUTION)
/* /< timeout of the Single ended special Autotune echo command */
/** @}*/

/**
  * Struct used to measure the time elapsed between a starting and ending point.
  */
typedef struct {
	struct timespec64 start; /* /< store the starting time */
	struct timespec64 end; /* /< store the finishing time */
} StopWatch;

void startStopWatch(StopWatch *w);
void stopStopWatch(StopWatch *w);
int elapsedMillisecond(StopWatch *w);
int elapsedNanosecond(StopWatch *w);

#endif
