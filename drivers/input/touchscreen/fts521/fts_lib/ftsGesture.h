/**
  ******************************************************************************
  * @file    ftsGesture.h
  * @brief   Contains all the macro and prototypes to handle the Gesture Detection
  * features
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

#ifndef FTS_GESTURE_H_
#define FTS_GESTURE_H_

#include "ftsHardware.h"

#define GESTURE_MASK_SIZE \
	4 /* /< number of bytes of the
						 * gesture mask */

#define GESTURE_MAX_COORDS_PAIRS_REPORT \
	100 /* /< max number of gestures
						 * coordinates pairs reported */

int updateGestureMask(u8 *mask, int size, int en);
int disableGesture(u8 *mask, int size);
int enableGesture(u8 *mask, int size);
int enterGestureMode(int reload);
int isAnyGestureActive(void);
int readGestureCoords(u8 *event);
int getGestureCoords(u16 **x, u16 **y);

#endif /* ! _GESTURE_H_ */
