/**
  ******************************************************************************
  * @file    fts.h
  * @brief   Contains all the definitions and structs used generally by the driver
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

#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

#include <linux/device.h>
#include "fts_lib/ftsSoftware.h"
#include "fts_lib/ftsHardware.h"

/****************** CONFIGURATION SECTION ******************/
/** @defgroup conf_section	 Driver Configuration Section
  * Settings of the driver code in order to suit the HW set up and the
  *application behavior
  * @{
  */
/* **** CODE CONFIGURATION **** */
#define FTS_TS_DRV_NAME "fts" /* /< driver name */
#define FTS_TS_DRV_VERSION "5.2.21" /* /< driver version string format */
#define FTS_TS_DRV_VER 0x05021500 /* driver version u32 format */

// #define DEBUG	/* /< define to print more logs in the kernel log and better
// 		 * follow the code flow */

#define PRE_SAVED_METHOD /* Pre-Saved Method used
				* during production */

#define USE_ONE_FILE_NODE
/* allow to enable/disable all the features just using one file node */

#define RETRY_INIT_BOOT \
	3 /* /< number of retry of the
					 * initialization process at boot */

#define PINCTRL_STATE_ACTIVE "default"
#define PINCTRL_STATE_SUSPEND "sleep"
/* **** END **** */

/* **** FEATURES USED IN THE IC **** */
/* Enable the support of keys */
/* #define PHONE_KEY */

#define CHARGER_MODE /* /< enable the support to charger mode feature
			 * (comment to disable) */

#define GLOVE_MODE /* /< enable the support to glove mode feature (comment
			 * to disable) */

#define COVER_MODE /* /< enable the support to cover mode feature (comment
			 * to disable) */

#define STYLUS_MODE /* /< enable the support to stylus mode feature (comment
			 * to disable) */

#define GRIP_MODE /* /< enable the support to grip mode feature (comment
			 * to disable) */

/* **** END **** */

/* **** PANEL SPECIFICATION **** */
#define X_AXIS_MAX 1080 /* /< Max X coordinate of the display */
#define X_AXIS_MIN 0 /* /< min X coordinate of the display */
#define Y_AXIS_MAX 2340 /* /< Max Y coordinate of the display */
#define Y_AXIS_MIN 0 /* /< min Y coordinate of the display */

#define PRESSURE_MIN 0 /* /< min value of pressure reported */
#define PRESSURE_MAX 127 /* /< Max value of pressure reported */

#define DISTANCE_MIN \
	0 /* /< min distance between the tool and the
				 * display */
#define DISTANCE_MAX \
	127 /* /< Max distance between the tool and the
				 * display */

#define TOUCH_ID_MAX \
	10 /* /< Max number of simoultaneous touches
				 * reported */

#define AREA_MIN \
	PRESSURE_MIN /* /< min value of Major/minor axis
					 * reported */
#define AREA_MAX \
	PRESSURE_MAX /* /< Man value of Major/minor axis
					 * reported */
/* **** END **** */
/**@}*/
/*********************************************************/

/*
  * Configuration mode
  *
  * bitmask which can assume the value defined as features in ftsSoftware.h or
  * the following values
  */

/** @defgroup mode_section	 IC Status Mode
  * Bitmask which keeps track of the features and working mode enabled in the
  * IC.
  * The meaning of the the LSB of the bitmask must be interpreted considering
  * that the value defined in @link feat_opt Feature Selection Option @endlink
  * correspond to the position of the corresponding bit in the mask
  * @{
  */
#define MODE_NOTHING 0x00000000 /* /< nothing enabled (sense off) */
#define MODE_ACTIVE(_mask, _sett) \
	(_mask |= (SCAN_MODE_ACTIVE << 24) | (_sett << 16))
/* /< store the status of scan mode active and its setting */
#define MODE_LOW_POWER(_mask, _sett) \
	(_mask |= (SCAN_MODE_LOW_POWER << 24) | (_sett << 16))
/* /< store the status of scan mode low power and its setting */
#define IS_POWER_MODE(_mask, _mode) ((_mask & (_mode << 24)) != 0x00)
/* /< check the current mode of the IC */

/** @}*/

#define CMD_STR_LEN \
	32 /* /< max number of parameters that can accept
				 * the MP file node (stm_fts_cmd) */

#define TSP_BUF_SIZE \
	PAGE_SIZE /* /< max number of bytes printable on
					 * the shell in the normal file nodes */

/**
  * Struct which contains information about the HW platform and set up
  */
struct fts_hw_platform_data {
	int (*power)(bool on);
	int irq_gpio; /* /< number of the gpio associated to the interrupt pin
			 * */
	int reset_gpio; /* /< number of the gpio associated to the reset pin */
	unsigned long irq_flags;
	unsigned int x_max;
	unsigned int y_max;
	const char *vdd_reg_name; /* /< name of the VDD regulator */
	const char *avdd_reg_name; /* /< name of the AVDD regulator */
};

/*
  * Forward declaration
  */
struct fts_ts_info;
extern char tag[8]; /* /< forward the definition of the label used
			  * to print the log in the kernel log */

/*
  * Dispatch event handler
  */
typedef void (*event_dispatch_handler_t)(struct fts_ts_info *info,
					 unsigned char *data);

/**
  * FTS capacitive touch screen device information
  * - dev             Pointer to the structure device \n
  * - client          client structure \n
  * - input_dev       Input device structure \n
  * - event_wq        Event queue for work thread \n
  * - event_dispatch_table  Event dispatch table handlers \n
  * - attrs           SysFS attributes \n
  * - mode            Device operating mode (bitmask) \n
  * - touch_id        Bitmask for touch id (mapped to input slots) \n
  * - stylus_id       Bitmask for tracking the stylus touches (mapped using the
  * touchId) \n
  * - timer           Timer when operating in polling mode \n
  * - power           Power on/off routine \n
  * - board           HW info retrieved from device tree \n
  * - vdd_reg         DVDD power regulator \n
  * - avdd_reg        AVDD power regulator \n
  * - resume_bit      Indicate if screen off/on \n
  * - sensor_sleep    true suspend was called, false resume was called \n
  * - input_report_mutex  mutex for handling the pressure of keys \n
  * - series_of_switches  to store the enabling status of a particular feature
  * from the host \n
  */
struct fts_ts_info {
	struct device *dev; /* /< Pointer to the structure device */
#ifdef I2C_INTERFACE
	struct i2c_client *client; /* /< I2C client structure */
#else
	struct spi_device *client; /* /< SPI client structure */
#endif
	struct input_dev *input_dev; /* /< Input device structure */

	struct work_struct suspend_work; /* /< Suspend work thread */
	struct work_struct resume_work; /* /< Resume work thread */
	struct workqueue_struct *event_wq; /* /< Workqueue used for event
						 * handler, suspend and resume
						 * work threads */

	event_dispatch_handler_t *event_dispatch_table; /* /< Event dispatch
							 * table handlers */

	struct attribute_group attrs; /* /< SysFS attributes */

	unsigned int mode; /* /< Device operating mode (bitmask: msb
				 * indicate if active or lpm) */
	unsigned long touch_id; /* /< Bitmask for touch id (mapped to input
				 * slots) */
#ifdef STYLUS_MODE
	unsigned long stylus_id; /* /< Bitmask for tracking the stylus
					 * touches (mapped using the touchId) */
#endif

	struct fts_hw_platform_data *board; /* /< HW info retrieved from
						 * device tree */
	struct regulator *vdd_reg; /* /< DVDD power regulator */
	struct regulator *avdd_reg; /* /< AVDD power regulator */

	int resume_bit; /* /< Indicate if screen off/on */

	bool sensor_sleep; /* /< if true suspend was called while if false
				 * resume was called */

	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;

	/* input lock */
	struct mutex input_report_mutex; /* /< mutex for handling the report
						 * of the pressure of keys */

	/* switches for features */
	int glove_enabled; /* /< if set, the glove mode will be enabled
				 * when allowed */
	int charger_enabled; /* /< if set, the charger mode will be enabled
				 * when allowed */
	int stylus_enabled; /* /< if set, the stylus mode will be enabled
				 * when allowed */
	int cover_enabled; /* /< if set, the cover mode will be enabled
				 * when allowed */
	int grip_enabled; /* /< if set, the grip mode mode will be enabled
				 * when allowed */
};

int fts_chip_powercycle(struct fts_ts_info *info);
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);

#endif
