/**
  ******************************************************************************
  * @file    fts.c
  * @brief   It is the main file which contains entry functions and event
  * handlers for input touch driver
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

#include <linux/device.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spi.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>

#include "fts.h"
#include "fts_lib/ftsCore.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"

/**
  * Event handler installer helpers
  */
#define event_id(_e) (EVT_ID_##_e >> 4)
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
	(_i->event_dispatch_table[event_id(_evt)] = handler_name(_hnd))

#define TYPE_B_PROTOCOL

extern SysInfo systemInfo;

char tag[8] = "[ FTS ]\0";
char fts_ts_phys[64]; /* /< buffer which store the input device name assigned
			 * by the kernel */
#ifdef USE_ONE_FILE_NODE
static int feature_feasibility = ERROR_OP_NOT_ALLOW;
#endif

#ifdef PHONE_KEY
static u8 key_mask = 0x00; /* /< store the last update of the key mask
				 * published by the IC */
#endif

extern spinlock_t fts_int;

static int fts_init_sensing(struct fts_ts_info *info);
static int fts_mode_handler(struct fts_ts_info *info, int force);
static int fts_chip_initialization(struct fts_ts_info *info, u8 init_type);

/**
  * Release all the touches in the linux input subsystem
  * @param info pointer to fts_ts_info which contains info about the device and
  * its hw setup
  */
static void release_all_touches(struct fts_ts_info *info)
{
	unsigned int type = MT_TOOL_FINGER;
	int i;

	for (i = 0; i < TOUCH_ID_MAX; i++) {
#ifdef STYLUS_MODE
		if (test_bit(i, &info->stylus_id))
			type = MT_TOOL_PEN;
		else
			type = MT_TOOL_FINGER;
#endif
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, type, 0);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	}
	input_sync(info->input_dev);
	info->touch_id = 0;
#ifdef STYLUS_MODE
	info->stylus_id = 0;
#endif
}

/**
  * @defgroup file_nodes Driver File Nodes
  * Driver publish a series of file nodes used to provide several utilities
  * to the host and give him access to different API.
  * @{
  */

/**
  * @defgroup device_file_nodes Device File Nodes
  * @ingroup file_nodes
  * Device File Nodes \n
  * There are several file nodes that are associated to the device and which
  * are designed to be used by the host to enable/disable features or trigger
  * some system specific actions \n
  * Usually their final path depend on the definition of device tree node of
  * the IC (e.g /sys/devices/soc.0/f9928000.i2c/i2c-6/6-0049)
  * @{
  */
/***************************************** UTILITIES
  * (current fw_ver/conf_id, active mode, file fw_ver/conf_id)
  ***************************************************/
/**
  * File node to show on terminal external release version in Little Endian \n
  * (first the less significant byte) \n
  * cat appid	show the external release version of the FW running in the IC
  */
static ssize_t fts_appid_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int error;
	char temp[100];

	error = snprintf(buf, PAGE_SIZE, "%s\n",
			 printHex("EXT Release = ", systemInfo.u8_releaseInfo,
				  EXTERNAL_RELEASE_INFO_SIZE, temp));

	return error;
}

/**
  * File node to show on terminal the mode that is active on the IC \n
  * cat mode_active		    to show the bitmask which indicate
  * the modes/features which are running on the IC in a specific instant of time
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1 = 1 byte in HEX format which represent the actual running scan mode
  * (@link scan_opt Scan Mode Options @endlink) \n
  * X2 = 1 byte in HEX format which represent the bitmask on which is running
  * the actual scan mode \n
  * X3X4 = 2 bytes in HEX format which represent a bitmask of the features that
  * are enabled at this moment (@link feat_opt Feature Selection Options
  * @endlink) \n
  * } = end byte
  * @see fts_mode_handler()
  */
static ssize_t fts_mode_active_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s Current mode active = %08X\n", tag, info->mode);
	return snprintf(buf, 14, "{ %08X }\n", info->mode);
}

/***************************************** FEATURES
 * ***************************************************/

/* TODO: edit this function according to the features policy to allow during
  * the screen on/off, following is shown an example but check always with ST
  * for more details */
#ifdef USE_ONE_FILE_NODE
/**
  * File node to enable some feature
  * echo XX 00/01 > feature_enable		to enable/disable XX
  * (possible values @link feat_opt Feature Selection Options @endlink) feature
  * cat feature_enable		to show the result of enabling/disabling process
  * echo XX 01/00 > feature_enable; cat feature_enable
  * to perform both actions stated before in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which represent an error code (00000000 =
  * no error) \n
  * } = end byte
  */
static ssize_t fts_feature_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char *p = (char *)buf;
	unsigned int temp, temp2;
	int res = OK;

	if ((count - 2 + 1) / 3 != 1)
		logError(
			1,
			"%s fts_feature_enable: Number of parameter wrong! %d > %d\n",
			tag, (count - 2 + 1) / 3, 1);
	else {
		if (sscanf(p, "%02X %02X ", &temp, &temp2) == 2) {
			p += 3;
			switch (temp) {

#ifdef GLOVE_MODE
			case FEAT_SEL_GLOVE:
				info->glove_enabled = temp2;
				logError(
					1,
					"%s fts_feature_enable: Glove Enabled = %d\n",
					tag, info->glove_enabled);

				break;
#endif

#ifdef STYLUS_MODE
			case FEAT_SEL_STYLUS:
				info->stylus_enabled = temp2;
				logError(
					1,
					"%s fts_feature_enable: Stylus Enabled = %d\n",
					tag, info->stylus_enabled);

				break;
#endif

#ifdef COVER_MODE
			case FEAT_SEL_COVER:
				info->cover_enabled = temp2;
				logError(
					1,
					"%s fts_feature_enable: Cover Enabled = %d\n",
					tag, info->cover_enabled);

				break;
#endif

#ifdef CHARGER_MODE
			case FEAT_SEL_CHARGER:
				info->charger_enabled = temp2;
				logError(
					1,
					"%s fts_feature_enable: Charger Enabled = %d\n",
					tag, info->charger_enabled);

				break;
#endif

#ifdef GRIP_MODE
			case FEAT_SEL_GRIP:
				info->grip_enabled = temp2;
				logError(
					1,
					"%s fts_feature_enable: Grip Enabled = %d\n",
					tag, info->grip_enabled);

				break;
#endif

			default:
				logError(
					1,
					"%s fts_feature_enable: Feature %08X not valid! ERROR %08X\n",
					tag, temp, ERROR_OP_NOT_ALLOW);
				res = ERROR_OP_NOT_ALLOW;
			}
			feature_feasibility = res;

			if (feature_feasibility >= OK)
				feature_feasibility = fts_mode_handler(info, 1);
			else
				logError(
					1,
					"%s %s: Call echo XX 00/01 > feature_enable with a correct feature value (XX)! ERROR %08X\n",
					tag, __func__, res);
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}
	return count;
}

static ssize_t fts_feature_enable_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;

	if (feature_feasibility < OK)
		logError(
			1,
			"%s %s: Call before echo XX 00/01 > feature_enable with a correct feature value (XX)! ERROR %08X\n",
			tag, __func__, feature_feasibility);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  feature_feasibility);
		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(
			1,
			"%s fts_feature_enable_show: Unable to allocate all_strbuff! ERROR %08X\n",
			tag, ERROR_ALLOC);

	feature_feasibility = ERROR_OP_NOT_ALLOW;
	return count;
}

#else

#ifdef GRIP_MODE
/**
  * File node to set the grip mode
  * echo 01/00 > grip_mode	to enable/disable glove mode \n
  * cat grip_mode		to show the status of the grip_enabled switch \n
  * echo 01/00 > grip_mode; cat grip_mode		to enable/disable grip
  *mode
  * and see the switch status in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which represent the value
  * info->grip_enabled (1 = enabled; 0= disabled) \n
  * } = end byte
  */
static ssize_t fts_grip_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: grip_enabled = %d\n", tag, __func__,
		 info->grip_enabled);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  info->grip_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_grip_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* in case of a different elaboration of the input, just modify this
	 * initial part of the code according to customer needs */
	if ((count + 1) / 3 != 1)
		logError(
			1,
			"%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (int)((count + 1) / 3), 1);
	else {
		if (sscanf(p, "%02X ", &temp) == 1) {
			p += 3;

			/* standard code that should be always used when a feature is enabled! */
			/* first step : check if the wanted feature can be enabled */
			/* second step: call fts_mode_handler to actually enable it */
			/* NOTE: Disabling a feature is always allowed by default */
			if (temp == FEAT_DISABLE) {
				info->grip_enabled = temp;
				res = fts_mode_handler(info, 1);
				if (res < OK)
					logError(
						1,
						"%s %s: Error during fts_mode_handler! ERROR %08X\n",
						tag, __func__, res);
			}
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}

	return count;
}
#endif

#ifdef CHARGER_MODE
/**
  * File node to set the glove mode
  * echo XX/00 > charger_mode		to value >0 to enable
  * (possible values: @link charger_opt Charger Options @endlink),
  * 00 to disable charger mode \n
  * cat charger_mode	to show the status of the charger_enabled switch \n
  * echo 01/00 > charger_mode; cat charger_mode		to enable/disable
  * charger mode and see the switch status in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which represent the value
  * info->charger_enabled (>0 = enabled; 0= disabled) \n
  * } = end byte
  */
static ssize_t fts_charger_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: charger_enabled = %d\n", tag, __func__,
		 info->charger_enabled);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  info->charger_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_charger_mode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* in case of a different elaboration of the input, just modify this
  * initial part of the code according to customer needs */
	if ((count + 1) / 3 != 1)
		logError(
			1,
			"%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (int)((count + 1) / 3), 1);
	else {
		if (sscanf(p, "%02X ", &temp) == 1) {
			p += 3;

			/* standard code that should be always used when a feature is enabled! */
			/* first step : check if the wanted feature can be enabled */
			/* second step: call fts_mode_handler to actually enable it */
			/* NOTE: Disabling a feature is always allowed by default */
			if (temp == FEAT_DISABLE) {
				info->charger_enabled = temp;
				res = fts_mode_handler(info, 1);
				if (res < OK)
					logError(
						1,
						"%s %s: Error during fts_mode_handler! ERROR %08X\n",
						tag, __func__, res);
			}
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}

	return count;
}
#endif

#ifdef GLOVE_MODE
/**
  * File node to set the glove mode
  * echo 01/00 > glove_mode	to enable/disable glove mode \n
  * cat glove_mode	to show the status of the glove_enabled switch \n
  * echo 01/00 > glove_mode; cat glove_mode	to enable/disable glove mode and
  *  see the switch status in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which represent the of value
  * info->glove_enabled (1 = enabled; 0= disabled) \n
  * } = end byte
  */
static ssize_t fts_glove_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: glove_enabled = %d\n", tag, __func__,
		 info->glove_enabled);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  info->glove_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_glove_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* in case of a different elaboration of the input, just modify this
  * initial part of the code according to customer needs */
	if ((count + 1) / 3 != 1)
		logError(
			1,
			"%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (int)((count + 1) / 3), 1);
	else {
		if (sscanf(p, "%02X ", &temp) == 1) {
			p += 3;

			/* standard code that should be always used when a feature is enabled! */
			/* first step : check if the wanted feature can be enabled */
			/* second step: call fts_mode_handler to actually enable it */
			/* NOTE: Disabling a feature is always allowed by default */
			if (temp == FEAT_DISABLE) {
				info->glove_enabled = temp;
				res = fts_mode_handler(info, 1);
				if (res < OK)
					logError(
						1,
						"%s %s: Error during fts_mode_handler! ERROR %08X\n",
						tag, __func__, res);
			}
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}

	return count;
}
#endif

#ifdef COVER_MODE
/* echo 01/00 > cover_mode     to enable/disable cover mode */
/* cat cover_mode	to show the status of the cover_enabled switch
 * (example output in the terminal = "AA00000001BB" if the switch is enabled) */
/* echo 01/00 > cover_mode; cat cover_mode	to enable/disable cover mode and
  * see the switch status in just one call */
/* NOTE: the cover can be handled also using a notifier, in this case the body
  * of these functions should be copied in the notifier callback */
/**
  * File node to set the cover mode
  * echo 01/00 > cover_mode	to enable/disable cover mode \n
  * cat cover_mode	to show the status of the cover_enabled switch \n
  * echo 01/00 > cover_mode; cat cover_mode	to enable/disable cover mode
  * and see the switch status in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which is the value of info->cover_enabled
  * (1 = enabled; 0= disabled)\n
  * } = end byte \n
  * NOTE: \n
  * the cover can be handled also using a notifier, in this case the body of
  * these functions should be copied in the notifier callback
  */
static ssize_t fts_cover_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: cover_enabled = %d\n", tag, __func__,
		 info->cover_enabled);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  info->cover_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_cover_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* in case of a different elaboration of the input, just modify this
  * initial part of the code according to customer needs */
	if ((count + 1) / 3 != 1)
		logError(
			1,
			"%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (int)((count + 1) / 3), 1);
	else {
		if (sscanf(p, "%02X ", &temp) == 1) {
			p += 3;

			/* standard code that should be always used when a feature is enabled! */
			/* first step : check if the wanted feature can be enabled */
			/* second step: call fts_mode_handler to actually enable it */
			/* NOTE: Disabling a feature is always allowed by default */
			if (temp == FEAT_DISABLE) {
				info->cover_enabled = temp;
				res = fts_mode_handler(info, 1);
				if (res < OK)
					logError(
						1,
						"%s %s: Error during fts_mode_handler! ERROR %08X\n",
						tag, __func__, res);
			}
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}

	return count;
}
#endif

#ifdef STYLUS_MODE
/**
  * File node to enable the stylus report
  * echo 01/00 > stylus_mode		to enable/disable stylus mode \n
  * cat stylus_mode	to show the status of the stylus_enabled switch \n
  * echo 01/00 > stylus_mode; cat stylus_mode	to enable/disable stylus mode
  * and see the switch status in just one call \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * X1X2X3X4 = 4 bytes in HEX format which is the value of info->stylus_enabled
  * (1 = enabled; 0= disabled)\n
  * } = end byte
  */
static ssize_t fts_stylus_mode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: stylus_enabled = %d\n", tag, __func__,
		 info->stylus_enabled);

	all_strbuff = (u8 *)kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index += snprintf(&all_strbuff[index], 13, "{ %08X }",
				  info->stylus_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_stylus_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* in case of a different elaboration of the input, just modify this
  * initial part of the code according to customer needs */
	if ((count + 1) / 3 != 1)
		logError(
			1,
			"%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (int)((count + 1) / 3), 1);
	else {
		if (sscanf(p, "%02X ", &temp) == 1) {
			p += 3;
			info->stylus_enabled = temp;
		} else
			logError(1, "%s %s: Error when reading with sscanf!\n",
				 tag, __func__);
	}

	return count;
}
#endif

#endif

static DEVICE_ATTR(appid, (S_IRUGO), fts_appid_show, NULL);
static DEVICE_ATTR(mode_active, (S_IRUGO), fts_mode_active_show, NULL);
#ifdef USE_ONE_FILE_NODE
static DEVICE_ATTR(feature_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_feature_enable_show, fts_feature_enable_store);
#else

#ifdef GRIP_MODE
static DEVICE_ATTR(grip_mode, (S_IRUGO | S_IWUSR | S_IWGRP), fts_grip_mode_show,
		   fts_grip_mode_store);
#endif

#ifdef CHARGER_MODE
static DEVICE_ATTR(charger_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_charger_mode_show, fts_charger_mode_store);
#endif

#ifdef GLOVE_MODE
static DEVICE_ATTR(glove_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_glove_mode_show, fts_glove_mode_store);
#endif

#ifdef COVER_MODE
static DEVICE_ATTR(cover_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_cover_mode_show, fts_cover_mode_store);
#endif

#ifdef STYLUS_MODE
static DEVICE_ATTR(stylus_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_stylus_mode_show, fts_stylus_mode_store);
#endif

#endif

/*  /sys/devices/soc.0/f9928000.i2c/i2c-6/6-0049 */
static struct attribute *fts_attr_group[] = {
	&dev_attr_appid.attr,
	&dev_attr_mode_active.attr,
#ifdef USE_ONE_FILE_NODE
	&dev_attr_feature_enable.attr,
#else

#ifdef GRIP_MODE
	&dev_attr_grip_mode.attr,
#endif
#ifdef CHARGER_MODE
	&dev_attr_charger_mode.attr,
#endif
#ifdef GLOVE_MODE
	&dev_attr_glove_mode.attr,
#endif
#ifdef COVER_MODE
	&dev_attr_cover_mode.attr,
#endif
#ifdef STYLUS_MODE
	&dev_attr_stylus_mode.attr,
#endif

#endif
	NULL,
};

/** @}*/
/** @}*/

/**
  * @defgroup isr Interrupt Service Routine (Event Handler)
  * The most important part of the driver is the ISR (Interrupt Service Routine)
  * called also as Event Handler \n
  * As soon as the interrupt pin goes low, fts_interrupt_handler() is called and
  * the chain to read and parse the event read from the FIFO start.\n
  * For any different kind of EVT_ID there is a specific event handler
  * which will take the correct action to report the proper info to the host. \n
  * The most important events are the one related to touch informations, status
  * update or user report.
  * @{
  */
#ifdef PHONE_KEY
/**
  * Report to the linux input system the pressure and release of a button
  * handling concurrency
  * @param info pointer to fts_ts_info which contains info about the device
  * and its hw setup
  * @param key_code	button value
  */
static void fts_input_report_key(struct fts_ts_info *info, int key_code)
{
	mutex_lock(&info->input_report_mutex);
	input_report_key(info->input_dev, key_code, 1);
	input_sync(info->input_dev);
	input_report_key(info->input_dev, key_code, 0);
	input_sync(info->input_dev);
	mutex_unlock(&info->input_report_mutex);
}
#endif

/**
  * Event Handler for no events (EVT_ID_NOEVENT)
  */
static void fts_nop_event_handler(struct fts_ts_info *info,
				  unsigned char *event)
{
	logError(
		0,
		"%s %s Doing nothing for event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3], event[4],
		event[5], event[6], event[7]);
}

/**
  * Event handler for enter and motion events (EVT_ID_ENTER_POINT,
  * EVT_ID_MOTION_POINT )
  * report to the linux input system touches with their coordinated and
  * additional informations
  */
static void fts_enter_pointer_event_handler(struct fts_ts_info *info,
					    unsigned char *event)
{
	unsigned char touchId;
	unsigned int touch_condition = 1, tool = MT_TOOL_FINGER;
	int x, y, z, distance;
	u8 touchType;

	if (!info->resume_bit)
		goto no_report;

	touchType = event[1] & 0x0F;
	touchId = (event[1] & 0xF0) >> 4;

	x = (((int)event[3] & 0x0F) << 8) | (event[2]);
	y = ((int)event[4] << 4) | ((event[3] & 0xF0) >> 4);
	/* TODO: check with fw how they will report distance and pressure */
	z = PRESSURE_MAX;
	distance = 0; /* if the tool is touching the display the distance
			 * should be 0 */

	if (x >= info->board->x_max)
		x = info->board->x_max;

	if (y >= info->board->y_max)
		y = info->board->y_max;

	input_mt_slot(info->input_dev, touchId);
	switch (touchType) {
#ifdef STYLUS_MODE
	case TOUCH_TYPE_STYLUS:
		logError(0, "%s  %s : It is a stylus!\n", tag, __func__);
		if (info->stylus_enabled == 1) { /* if stylus_enabled is
							 * not ==1 it will be
							 * reported as normal
							 * touch */
			tool = MT_TOOL_PEN;
			touch_condition = 1;
			__set_bit(touchId, &info->stylus_id);
			break;
		}
		fallthrough;
#endif
	/* TODO: customer can implement a different strategy for each kind of
	 * touch */
	case TOUCH_TYPE_FINGER:
	/* logError(0, "%s  %s : It is a finger!\n",tag,__func__); */
	case TOUCH_TYPE_GLOVE:
	/* logError(0, "%s  %s : It is a glove!\n",tag,__func__); */
	case TOUCH_TYPE_PALM:
		/* logError(0, "%s  %s : It is a palm!\n",tag,__func__); */
		tool = MT_TOOL_FINGER;
		touch_condition = 1;
		__set_bit(touchId, &info->touch_id);
		break;

	case TOUCH_TYPE_HOVER:
		tool = MT_TOOL_FINGER;
		touch_condition = 0; /* need to hover */
		z = 0; /* no pressure */
		__set_bit(touchId, &info->touch_id);
		distance = DISTANCE_MAX; /* check with fw report the
						 * hovering distance */
		break;

	case TOUCH_TYPE_INVALID:
	default:
		logError(0, "%s  %s : Invalid touch type = %d ! No Report...\n",
			 tag, __func__, touchType);
		goto no_report;
	}

	input_mt_report_slot_state(info->input_dev, tool, 1);

	/* logError(0, "%s  %s : TouchID = %d,Touchcount = %d
	 *\n",tag,__func__,touchId,touchcount); */
	input_report_key(info->input_dev, BTN_TOUCH, touch_condition);

	/* input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId); */
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);
	input_report_abs(info->input_dev, ABS_MT_DISTANCE, distance);
	/* logError(0, "%s  %s :  Event 0x%02x - ID[%d], (x, y) = (%3d, %3d)
	 * Size = %d\n", tag, __func__, *event, touchId, x, y, touchType); */

no_report:
	return;
}

/**
  * Event handler for leave event (EVT_ID_LEAVE_POINT )
  * Report to the linux input system that one touch left the display
  */
static void fts_leave_pointer_event_handler(struct fts_ts_info *info,
					    unsigned char *event)
{
	unsigned char touchId;
	unsigned int tool = MT_TOOL_FINGER;
	u8 touchType;

	touchType = event[1] & 0x0F;
	touchId = (event[1] & 0xF0) >> 4;

	input_mt_slot(info->input_dev, touchId);
	switch (touchType) {
#ifdef STYLUS_MODE
	case TOUCH_TYPE_STYLUS:
		logError(0, "%s  %s : It is a stylus!\n", tag, __func__);
		if (info->stylus_enabled == 1) {
			/* if stylus_enabled is not ==1 it will be reported as
			 * normal touch */
			tool = MT_TOOL_PEN;
			__clear_bit(touchId, &info->stylus_id);
			break;
		}
		fallthrough;
#endif

	case TOUCH_TYPE_FINGER:
	/* logError(0, "%s  %s : It is a finger!\n",tag,__func__); */
	case TOUCH_TYPE_GLOVE:
	/* logError(0, "%s  %s : It is a glove!\n",tag,__func__); */
	case TOUCH_TYPE_PALM:
	/* logError(0, "%s  %s : It is a palm!\n",tag,__func__); */
	case TOUCH_TYPE_HOVER:
		tool = MT_TOOL_FINGER;
		__clear_bit(touchId, &info->touch_id);
		break;

	case TOUCH_TYPE_INVALID:
	default:
		logError(0, "%s  %s : Invalid touch type = %d ! No Report...\n",
			 tag, __func__, touchType);
		return;
	}

	input_mt_report_slot_state(info->input_dev, tool, 0);

	/* logError(0, "%s  %s : TouchID = %d, Touchcount = %d\n",tag,__func__,
	  *	touchId,touchcount); */

	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	/* logError(0, "%s  %s : Event 0x%02x - release ID[%d]\n", tag,
	 * __func__, event[0], touchId); */
}

/* EventId : EVT_ID_MOTION_POINT */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler
/* remap the motion event handler to the same function which handle the enter
 * event */

/**
  * Event handler for error events (EVT_ID_ERROR)
  * Handle unexpected error events implementing recovery strategy and
  * restoring the sensing status that the IC had before the error occured
  */
static void fts_error_event_handler(struct fts_ts_info *info,
				    unsigned char *event)
{
	int error = 0;

	logError(
		0,
		"%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3], event[4],
		event[5], event[6], event[7]);

	switch (event[1]) {
	case EVT_TYPE_ERROR_ESD: /* esd */
	{ /* before reset clear all slot */
		release_all_touches(info);

		fts_chip_powercycle(info);

		error = fts_system_reset();
		error |= fts_mode_handler(info, 0);
		error |= fts_enableInterrupt();
		if (error < OK)
			logError(1,
				 "%s %s Cannot restore the device ERROR %08X\n",
				 tag, __func__, error);
	} break;

	case EVT_TYPE_ERROR_HARD_FAULT: /* hard fault */
	case EVT_TYPE_ERROR_WATCHDOG: /* watch dog timer */
	{
		dumpErrorInfo(NULL, 0);
		/* before reset clear all slots */
		release_all_touches(info);
		error = fts_system_reset();
		error |= fts_mode_handler(info, 0);
		error |= fts_enableInterrupt();
		if (error < OK)
			logError(1,
				 "%s %s Cannot reset the device ERROR %08X\n",
				 tag, __func__, error);
	} break;
	}
}

/**
  * Event handler for controller ready event (EVT_ID_CONTROLLER_READY)
  * Handle controller events received after unexpected reset of the IC updating
  * the resets flag and restoring the proper sensing status
  */
static void fts_controller_ready_event_handler(struct fts_ts_info *info,
					       unsigned char *event)
{
	int error;

	logError(
		0,
		"%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3], event[4],
		event[5], event[6], event[7]);
	release_all_touches(info);
	setSystemResetedUp(1);
	setSystemResetedDown(1);
	error = fts_mode_handler(info, 0);
	if (error < OK)
		logError(1,
			 "%s %s Cannot restore the device status ERROR %08X\n",
			 tag, __func__, error);
}

/**
  * Event handler for status events (EVT_ID_STATUS_UPDATE)
  * Handle status update events
  */
static void fts_status_event_handler(struct fts_ts_info *info,
				     unsigned char *event)
{
	switch (event[1]) {
	case EVT_TYPE_STATUS_ECHO:
		logError(
			0,
			"%s %s Echo event of command = %02X %02X %02X %02X %02X %02X\n",
			tag, __func__, event[2], event[3], event[4], event[5],
			event[6], event[7]);
		break;

	case EVT_TYPE_STATUS_FORCE_CAL:
		switch (event[2]) {
		case 0x00:
			logError(
				0,
				"%s %s Continuous frame drop Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x01:
			logError(
				0,
				"%s %s Sense On Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x02:
			logError(
				0,
				"%s %s Host command Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x10:
			logError(
				0,
				"%s %s Mutual frame drop Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x11:
			logError(
				0,
				"%s %s Mutual pure raw Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x20:
			logError(
				0,
				"%s %s SS detect negative Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x30:
			logError(
				0,
				"%s %s invalid Mutual soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x31:
			logError(
				0,
				"%s %s invalid Self soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x32:
			logError(
				0,
				"%s %s ss invalid island soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		default:
			logError(
				0,
				"%s %s Force cal = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		}
		break;

	case EVT_TYPE_STATUS_FRAME_DROP:
		switch (event[2]) {
		case 0x01:
			logError(
				0,
				"%s %s Frame drop noisy frame = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x02:
			logError(
				0,
				"%s %s Frame drop bad R = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		case 0x03:
			logError(
				0,
				"%s %s Frame drop invalid processing state = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
			break;

		default:
			logError(
				0,
				"%s %s Frame drop = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		}
		break;

	case EVT_TYPE_STATUS_SS_RAW_SAT:
		if (event[2] == 1)
			logError(
				0,
				"%s %s SS Raw Saturated = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		else
			logError(
				0,
				"%s %s SS Raw No more Saturated = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		break;

	case EVT_TYPE_STATUS_WATER:
		if (event[2] == 1)
			logError(
				0,
				"%s %s Enter Water mode = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		else
			logError(
				0,
				"%s %s Exit Water mode = %02X %02X %02X %02X %02X %02X\n",
				tag, __func__, event[2], event[3], event[4],
				event[5], event[6], event[7]);
		break;

	default:
		logError(
			0,
			"%s %s Received unhandled status event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			tag, __func__, event[0], event[1], event[2], event[3],
			event[4], event[5], event[6], event[7]);
		break;
	}
}

/* key events reported in the user report */
#ifdef PHONE_KEY
/* TODO: the customer should handle the events coming from the keys according
 * his needs (this is just an sample code that report the click of a botton
 * after a press->release action) */
/**
  * Event handler for status events (EVT_TYPE_USER_KEY)
  * Handle keys update events, the third byte of the event is a bitmask where if
  *the bit set means that the corresponding key is pressed.
  */
static void fts_key_event_handler(struct fts_ts_info *info,
				  unsigned char *event)
{
	/* int value; */
	logError(
		0,
		"%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3], event[4],
		event[5], event[6], event[7]);

	if (event[0] == EVT_ID_USER_REPORT && event[1] == EVT_TYPE_USER_KEY) {
		/* event[2] contain the bitmask of the keys that are actually
		 * pressed */

		if ((event[2] & FTS_KEY_0) == 0 && (key_mask & FTS_KEY_0) > 0) {
			logError(0,
				 "%s %s: Button HOME pressed and released!\n",
				 tag, __func__);
			fts_input_report_key(info, KEY_HOMEPAGE);
		}

		if ((event[2] & FTS_KEY_1) == 0 && (key_mask & FTS_KEY_1) > 0) {
			logError(0,
				 "%s %s: Button Back pressed and released!\n",
				 tag, __func__);
			fts_input_report_key(info, KEY_BACK);
		}

		if ((event[2] & FTS_KEY_2) == 0 && (key_mask & FTS_KEY_2) > 0) {
			logError(0, "%s %s: Button Menu pressed!\n", tag,
				 __func__);
			fts_input_report_key(info, KEY_MENU);
		}

		key_mask = event[2];
	} else
		logError(1, "%s %s: Invalid event passed as argument!\n", tag,
			 __func__);
}
#endif

/**
  * Event handler for user report events (EVT_ID_USER_REPORT)
  * Handle user events reported by the FW due to some interaction triggered
  * by an external user (press keys, perform gestures, etc.)
  */
static void fts_user_report_event_handler(struct fts_ts_info *info,
					  unsigned char *event)
{
	switch (event[1]) {
#ifdef PHONE_KEY
	case EVT_TYPE_USER_KEY:
		fts_key_event_handler(info, event);
		break;
#endif

	case EVT_TYPE_USER_PROXIMITY:
		if (event[2] == 0)
			logError(0, "%s %s No proximity!\n", tag, __func__);
		else
			logError(0, "%s %s Proximity Detected!\n", tag,
				 __func__);
		break;

	default:
		logError(
			0,
			"%s %s Received unhandled user report event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			tag, __func__, event[0], event[1], event[2], event[3],
			event[4], event[5], event[6], event[7]);
		break;
	}
}

/**
  * Bottom Half Interrupt Handler function
  * This handler is called each time there is at least one new event in the FIFO
  * and the interrupt pin of the IC goes low. It will read all the events from
  * the FIFO and dispatch them to the proper event handler according the event
  * ID
  */
static irqreturn_t fts_event_handler(int irq, void *ts_info)
{
	struct fts_ts_info *info = ts_info;
	int error = 0, count = 0;
	unsigned char regAdd;
	unsigned char data[FIFO_EVENT_SIZE] = { 0 };
	unsigned char eventId;

	event_dispatch_handler_t event_handler;

	pm_wakeup_event(info->dev, jiffies_to_msecs(HZ));

	/* read the FIFO and parsing events */

	regAdd = FIFO_CMD_READONE;

	for (count = 0; count < FIFO_DEPTH; count++) {
		error = fts_writeReadU8UX(regAdd, 0, 0, data, FIFO_EVENT_SIZE,
					  DUMMY_FIFO);

		/*logError(0, "%s %s event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			tag, __func__, data[0],
			data[1], data[2], data[3], data[4], data[5], data[6],
			data[7]);*/
		if (error == OK && data[0] != EVT_ID_NOEVENT)
			eventId = data[0] >> 4;
		else
			break;
		/* if(data[7]&0x20) */
		/* logError(1, "%s %s overflow ID = %02X  Last = %02X\n", tag,
		 * __func__, data[0], data[7]); */

		if (eventId < NUM_EVT_ID) { /* this check prevent array out
						 * of index in case of no sense
						 * event ID */
			event_handler = info->event_dispatch_table[eventId];
			event_handler(info, (data));
		}
	}
	input_sync(info->input_dev);
	return IRQ_HANDLED;
}
/** @}*/

/**
  *	Implement the initialization flow of the IC that should be
  *executed at every boot up.
  *	The function perform a fw update of the IC in case of crc error or a new
  *fw version and then understand if the IC need to be re-initialized again.
  *	@return  OK if success or an error code which specify the type of error
  *	encountered
  */
static int fts_init_flow(struct fts_ts_info *info)
{
	u8 error_to_search[4] = { EVT_TYPE_ERROR_CRC_CX_HEAD,
				  EVT_TYPE_ERROR_CRC_CX,
				  EVT_TYPE_ERROR_CRC_CX_SUB_HEAD,
				  EVT_TYPE_ERROR_CRC_CX_SUB };
	int ret;
	int error = 0;
	u8 init_type = 0;

	logError(0, "%s %s: Verifying if CX CRC Error...\n", tag, __func__);
	ret = fts_system_reset();
	if (ret >= OK) {
		ret = pollForErrorType(error_to_search, 4);
		if (ret < OK) {
			logError(0, "%s %s: No CX CRC Error Found!\n", tag,
				 __func__);
			logError(0, "%s %s: Verifying if Panel CRC Error...\n",
				 tag, __func__);
			error_to_search[0] = EVT_TYPE_ERROR_CRC_PANEL_HEAD;
			error_to_search[1] = EVT_TYPE_ERROR_CRC_PANEL;
			ret = pollForErrorType(error_to_search, 2);
			if (ret < OK) {
				logError(0,
					 "%s %s: No Panel CRC Error Found!\n",
					 tag, __func__);
				init_type = 0;
			} else {
				logError(
					1,
					"%s %s: Panel CRC Error FOUND! CRC ERROR = %02X\n",
					tag, __func__, ret);
				init_type = SPECIAL_PANEL_INIT;
			}
		} else {
			logError(
				1,
				"%s %s: CX CRC Error FOUND! CRC ERROR = %02X\n",
				tag, __func__, ret);
			logError(1, "%s %s: Select Full Panel Init...\n", tag,
				 __func__);
			init_type = SPECIAL_FULL_PANEL_INIT;
		}
	} else
		logError(
			1,
			"%s %s: Error while executing system reset! ERROR %08X\n",
			tag, __func__, ret); /* better skip initialization
						 * because the real state is
						 * unknown */

	if (init_type != SPECIAL_FULL_PANEL_INIT) {
#ifdef PRE_SAVED_METHOD
		if ((systemInfo.u8_cfgAfeVer != systemInfo.u8_cxAfeVer)) {
			init_type = SPECIAL_FULL_PANEL_INIT;
			logError(
				0,
				"%s %s: Different CX AFE Ver: %02X != %02X or invalid MpFlag = %02X... Execute FULL Panel Init!\n",
				tag, __func__, systemInfo.u8_cfgAfeVer,
				systemInfo.u8_cxAfeVer, systemInfo.u8_mpFlag);
		} else
#endif
			if (systemInfo.u8_cfgAfeVer !=
			    systemInfo.u8_panelCfgAfeVer) {
			init_type = SPECIAL_PANEL_INIT;
			logError(
				0,
				"%s %s: Different Panel AFE Ver: %02X != %02X... Execute Panel Init!\n",
				tag, __func__, systemInfo.u8_cfgAfeVer,
				systemInfo.u8_panelCfgAfeVer);
		} else
			init_type = 0;
	}

	if (init_type != 0) { /* initialization status not correct or
					 * after FW complete update, do
					 * initialization. */
		error = fts_chip_initialization(info, init_type);
		if (error < OK)
			logError(
				1,
				"%s %s Cannot initialize the chip ERROR %08X\n",
				tag, __func__, error);
	}

	error = fts_init_sensing(info);
	if (error < OK)
		logError(
			1,
			"%s Cannot initialize the hardware device ERROR %08X\n",
			tag, error);

	return error;
}

/**
  *	Execute the initialization of the IC (supporting a retry mechanism),
  * checking also the resulting data
  */
static int fts_chip_initialization(struct fts_ts_info *info, u8 init_type)
{
	int res = 0;
	int retry;

	logError(0, "%s Chip initialization is starting...\n", tag);
	if (init_type != SPECIAL_PANEL_INIT &&
	    init_type != SPECIAL_FULL_PANEL_INIT) {
		logError(1,
			 "%s %s: Type incompatible! Type = %02X ERROR %08X\n",
			 tag, __func__, init_type,
			 ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_INITIALIZATION);
		return ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_INITIALIZATION;
	}

	/* initialization error, retry initialization */
	for (retry = 0; retry < RETRY_INIT_BOOT; retry++) {
		res = fts_system_reset();
		if (res < 0) {
			logError(1, "%s %s: ERROR %08X\n", tag, __func__,
				 ERROR_PROD_TEST_INITIALIZATION);
			res |= ERROR_PROD_TEST_INITIALIZATION;
			goto retry;
		}

		logError(0, "%s INITIALIZATION command sent... %02X\n", tag,
			 init_type);
		res = writeSysCmd(SYS_CMD_SPECIAL, &init_type, 1);
		if (res < OK) {
			logError(1, "%s %s: ERROR %08X\n", tag, __func__,
				 (res | ERROR_PROD_TEST_INITIALIZATION));
			res |= ERROR_PROD_TEST_INITIALIZATION;
			goto retry;
		}

		logError(0, "%s Refresh Sys Info...\n", tag);
		res |= readSysInfo(1); /* need to update the chipInfo in order
					* to refresh several versions */

		if (res < 0) {
			logError(1, "%s %s: read sys info ERROR %08X\n", tag,
				 __func__, ERROR_PROD_TEST_INITIALIZATION);
			res |= ERROR_PROD_TEST_INITIALIZATION;
			goto retry;
		}

		break;
retry:
		logError(1,
			 "%s initialization cycle count = %04d - ERROR %08X\n",
			 tag, retry, res);
		fts_chip_powercycle(info);
	}

	if (res < OK) /* initialization error */
		logError(1, "%s fts initialization failed %d times\n", tag,
			 RETRY_INIT_BOOT);

	return res;
}

/**
  * Initialize the dispatch table with the event handlers for any possible event
  * ID
  * Set IRQ pin behavior (level triggered low)
  * Register top half interrupt handler function.
  * @see fts_interrupt_handler()
  */
static int fts_interrupt_install(struct fts_ts_info *info)
{
	int i, error = 0;

	info->event_dispatch_table = kzalloc(
		sizeof(event_dispatch_handler_t) * NUM_EVT_ID, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		logError(1, "%s OOM allocating event dispatch table\n", tag);
		return -ENOMEM;
	}

	for (i = 0; i < NUM_EVT_ID; i++)
		info->event_dispatch_table[i] = fts_nop_event_handler;

	install_handler(info, ENTER_POINT, enter_pointer);
	install_handler(info, LEAVE_POINT, leave_pointer);
	install_handler(info, MOTION_POINT, motion_pointer);
	install_handler(info, ERROR, error);
	install_handler(info, CONTROLLER_READY, controller_ready);
	install_handler(info, STATUS_UPDATE, status);
	install_handler(info, USER_REPORT, user_report);

	/* disable interrupts in any case */
	error = fts_disableInterrupt();

	logError(0, "%s Interrupt Mode\n", tag);
	if (request_threaded_irq(info->client->irq, NULL, fts_event_handler,
				 info->board->irq_flags, FTS_TS_DRV_NAME,
				 info)) {
		logError(1, "%s Request irq failed\n", tag);
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	}

	return error;
}

/**
  *	Clean the dispatch table and the free the IRQ.
  *	This function is called when the driver need to be removed
  */
static void fts_interrupt_uninstall(struct fts_ts_info *info)
{
	fts_disableInterrupt();

	kfree(info->event_dispatch_table);

	free_irq(info->client->irq, info);
}

/**@}*/

/**
  * This function try to attempt to communicate with the IC for the first time
  * during the boot up process in order to read the necessary info for the
  * following stages.
  * The function execute a system reset, read fundamental info (system info)
  * @return OK if success or an error code which specify the type of error
  */
static int fts_init(struct fts_ts_info *info)
{
	int error;
	u8 readData[3];

#if !defined(I2C_INTERFACE) && defined(SPI4_WIRE)
	/* configure manually SPI4 because when no fw is running the chip use
	 * SPI3 by default */
	u8 cmd[1] = { 0x00 };

	logError(0, "%s Setting SPI4 mode...\n", tag);
	cmd[0] = 0x10;
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG,
			      ADDR_GPIO_DIRECTION, cmd, 1);
	if (error < OK) {
		logError(1, "%s can not set gpio dir ERROR %08X\n", tag, error);
		return error;
	}

	cmd[0] = 0x02;
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG,
			      ADDR_GPIO_PULLUP, cmd, 1);
	if (error < OK) {
		logError(1, "%s can not set gpio pull-up ERROR %08X\n", tag,
			 error);
		return error;
	}

#if defined(ALIX) || defined(SALIXP)
#if defined(ALIX)
	cmd[0] = 0x70;
#else
	cmd[0] = 0x07;
#endif
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG,
			      ADDR_GPIO_CONFIG_REG3, cmd, 1);
	if (error < OK) {
		logError(1, "%s can not set gpio config ERROR %08X\n", tag,
			 error);
		return error;
	}

#else
	cmd[0] = 0x07;
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG,
			      ADDR_GPIO_CONFIG_REG2, cmd, 1);
	if (error < OK) {
		logError(1, "%s can not set gpio config ERROR %08X\n", tag,
			 error);
		return error;
	}
#endif

	cmd[0] = 0x30;
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG,
			      ADDR_GPIO_CONFIG_REG0, cmd, 1);
	if (error < OK) {
		logError(1, "%s can not set gpio config ERROR %08X\n", tag,
			 error);
		return error;
	}

	cmd[0] = SPI4_MASK;
	error = fts_writeU8UX(FTS_CMD_HW_REG_W, ADDR_SIZE_HW_REG, ADDR_ICR, cmd,
			      1);
	if (error < OK) {
		logError(1, "%s can not set spi4 mode ERROR %08X\n", tag,
			 error);
		return error;
	}
	msleep(1); /* wait for the GPIO to stabilize */
#endif
	logError(0, "%s Reading chip id\n", tag);
	error = fts_writeReadU8UX(FTS_CMD_HW_REG_R, ADDR_SIZE_HW_REG,
				  ADDR_DCHIP_ID, readData, 2, DUMMY_FIFO);
	logError(1, "%s chip id: %02X %02X!\n", tag, readData[0], readData[1]);

	if ((readData[0] != DCHIP_ID_0) || (readData[1] != DCHIP_ID_1)) {
		logError(1, "%s wrong chip id detected!\n", tag);
		return ERROR_OP_NOT_ALLOW;
	} else
		logError(0, "%s chip id read successful!\n", tag);

	error = fts_system_reset();
	if (error < OK && isI2cError(error)) {
		logError(1, "%s Cannot reset the device! ERROR %08X\n", tag,
			 error);
		return error;
	} else {
		if (error == (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
			logError(0, "%s Setting default Sys INFO!\n", tag);
			error = defaultSysInfo(0);
		} else {
			error = readSysInfo(0); /* system reset OK */
			if (error < OK) {
				if (!isI2cError(error))
					error = OK;
				logError(
					1,
					"%s Cannot read Sys Info! ERROR %08X\n",
					tag, error);
			}
		}
	}

	return error;
}

/**
  * Execute a power cycle in the IC, toggling the power lines (AVDD and DVDD)
  * @param info pointer to fts_ts_info struct which contain information of the
  * regulators
  * @return 0 if success or another value if fail
  */
int fts_chip_powercycle(struct fts_ts_info *info)
{
	int error = 0;

	logError(0, "%s %s: Power Cycle Starting...\n", tag, __func__);
	logError(0, "%s %s: Disabling IRQ...\n", tag, __func__);
	/* if IRQ pin is short with DVDD a call to the ISR will triggered when
	  * the regulator is turned off if IRQ not disabled */
	fts_disableInterrupt();

	if (info->vdd_reg) {
		error = regulator_disable(info->vdd_reg);
		if (error < 0)
			logError(0, "%s %s: Failed to disable DVDD regulator\n",
				 tag, __func__);
	}

	if (info->avdd_reg) {
		error = regulator_disable(info->avdd_reg);
		if (error < 0)
			logError(0, "%s %s: Failed to disable AVDD regulator\n",
				 tag, __func__);
	}

	if (info->board->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->board->reset_gpio, 0);
	else
		msleep(300);

	/* in FTI power up first the digital and then the analog */
	if (info->vdd_reg) {
		error = regulator_enable(info->vdd_reg);
		if (error < 0)
			logError(0, "%s %s: Failed to enable DVDD regulator\n",
				 tag, __func__);
	}

	msleep(1);

	if (info->avdd_reg) {
		error = regulator_enable(info->avdd_reg);
		if (error < 0)
			logError(0, "%s %s: Failed to enable AVDD regulator\n",
				 tag, __func__);
	}

	msleep(5); /* time needed by the regulators for reaching the regime
			 * values */

	if (info->board->reset_gpio != GPIO_NOT_DEFINED) {
		msleep(10); /* time to wait before bring up the reset gpio
				 * after the power up of the regulators */
		gpio_set_value(info->board->reset_gpio, 1);
	}

	release_all_touches(info);

	logError(0, "%s %s: Power Cycle Finished! ERROR CODE = %08x\n", tag,
		 __func__, error);
	setSystemResetedUp(1);
	setSystemResetedDown(1);
	return error;
}

/**
  * Complete the boot up process, initializing the sensing of the IC according
  * to the current setting chosen by the host
  * Register the notifier for the suspend/resume actions and the event handler
  * @return OK if success or an error code which specify the type of error
  */
static int fts_init_sensing(struct fts_ts_info *info)
{
	int error = 0;

	error |= fts_interrupt_install(info); /* register event handler */
	error |= fts_mode_handler(info, 0); /* enable the features and
						 * sensing */
	/* error |= fts_enableInterrupt(); */ /* enable the interrupt */
	error |= fts_resetDisableIrqCount();

	if (error < OK)
		logError(1, "%s %s Init after Probe error (ERROR = %08X)\n",
			 tag, __func__, error);

	return error;
}

/* TODO: change this function according with the needs of customer in terms of
 * feature to enable/disable */

/**
  * @ingroup mode_section
  * @{
  */
/**
  * The function handle the switching of the mode in the IC enabling/disabling
  * the sensing and the features set from the host
  * @param info pointer to fts_ts_info which contains info about the device and
  * its hw setup
  * @param force if 1, the enabling/disabling command will be send even
  * if the feature was already enabled/disabled otherwise it will judge if
  * the feature changed status or the IC had a system reset
  * @return OK if success or an error code which specify the type of error
  *encountered
  */
static int fts_mode_handler(struct fts_ts_info *info, int force)
{
	int res = OK;
	int ret = OK;
	u8 settings[4] = { 0 };

	/* disable irq wake because resuming from gesture mode */
	if (IS_POWER_MODE(info->mode, SCAN_MODE_LOW_POWER) &&
	    (info->resume_bit == 1))
		disable_irq_wake(info->client->irq);

	info->mode = MODE_NOTHING; /* initialize the mode to nothing in
					 * order to be updated depending on the
					 * features enabled */

	logError(0, "%s %s: Mode Handler starting...\n", tag, __func__);
	switch (info->resume_bit) {
	case 0: /* screen down */
		logError(0, "%s %s: Screen OFF...\n", tag, __func__);
		/* do sense off in order to avoid the flooding of the fifo with
		 * touch events if someone is touching the panel during suspend
		 */
		logError(0, "%s %s: Sense OFF!\n", tag, __func__);
		/* for speed reason (no need to check echo in this case and
		 * interrupt can be enabled) */
		ret = setScanMode(SCAN_MODE_ACTIVE, 0x00);
		res |= ret; /* to avoid warning unsused ret variable when
				 * all the features are disabled */

		setSystemResetedDown(0);
		break;

	case 1: /* screen up */
		logError(0, "%s %s: Screen ON...\n", tag, __func__);
#ifdef GLOVE_MODE
		if ((info->glove_enabled == FEAT_ENABLE &&
		     isSystemResettedUp()) ||
		    force == 1) {
			logError(0, "%s %s: Glove Mode setting...\n", tag,
				 __func__);
			settings[0] = info->glove_enabled;
			/* required to satisfy also the disable case */
			ret = setFeatures(FEAT_SEL_GLOVE, settings, 1);
			if (ret < OK)
				logError(
					1,
					"%s %s: error during setting GLOVE_MODE! ERROR %08X\n",
					tag, __func__, ret);
			res |= ret;

			if (ret >= OK && info->glove_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_GLOVE, (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(0, "%s %s: GLOVE_MODE Enabled!\n", tag,
					 __func__);
			} else
				logError(0, "%s %s: GLOVE_MODE Disabled!\n",
					 tag, __func__);
		}

#endif

#ifdef COVER_MODE
		if ((info->cover_enabled == FEAT_ENABLE &&
		     isSystemResettedUp()) ||
		    force == 1) {
			logError(0, "%s %s: Cover Mode setting...\n", tag,
				 __func__);
			settings[0] = info->cover_enabled;
			ret = setFeatures(FEAT_SEL_COVER, settings, 1);
			if (ret < OK)
				logError(
					1,
					"%s %s: error during setting COVER_MODE! ERROR %08X\n",
					tag, __func__, ret);
			res |= ret;

			if (ret >= OK && info->cover_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_COVER, (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(0, "%s %s: COVER_MODE Enabled!\n", tag,
					 __func__);
			} else
				logError(0, "%s %s: COVER_MODE Disabled!\n",
					 tag, __func__);
		}
#endif
#ifdef CHARGER_MODE
		if ((info->charger_enabled > 0 && isSystemResettedUp()) ||
		    force == 1) {
			logError(0, "%s %s: Charger Mode setting...\n", tag,
				 __func__);

			settings[0] = info->charger_enabled;
			ret = setFeatures(FEAT_SEL_CHARGER, settings, 1);
			if (ret < OK)
				logError(
					1,
					"%s %s: error during setting CHARGER_MODE! ERROR %08X\n",
					tag, __func__, ret);
			res |= ret;

			if (ret >= OK && info->charger_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_CHARGER,
					     (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(0, "%s %s: CHARGER_MODE Enabled!\n",
					 tag, __func__);
			} else
				logError(0, "%s %s: CHARGER_MODE Disabled!\n",
					 tag, __func__);
		}
#endif

#ifdef GRIP_MODE
		if ((info->grip_enabled == FEAT_ENABLE &&
		     isSystemResettedUp()) ||
		    force == 1) {
			logError(0, "%s %s: Grip Mode setting...\n", tag,
				 __func__);
			settings[0] = info->grip_enabled;
			ret = setFeatures(FEAT_SEL_GRIP, settings, 1);
			if (ret < OK)
				logError(
					1,
					"%s %s: error during setting GRIP_MODE! ERROR %08X\n",
					tag, __func__, ret);
			res |= ret;

			if (ret >= OK && info->grip_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_GRIP, (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(0, "%s %s: GRIP_MODE Enabled!\n", tag,
					 __func__);
			} else
				logError(0, "%s %s: GRIP_MODE Disabled!\n", tag,
					 __func__);
		}
#endif
		/* if some selective scan want to be enabled can be done an or
		 * of the following options */
		/* settings[0] = ACTIVE_MULTI_TOUCH | ACTIVE_KEY | ACTIVE_HOVER
		 * | ACTIVE_PROXIMITY | ACTIVE_FORCE; */
		settings[0] = 0xFF; /* enable all the possible scans mode
					 * supported by the config */
		logError(0, "%s %s: Sense ON!\n", tag, __func__);
		res |= setScanMode(SCAN_MODE_ACTIVE, settings[0]);
		info->mode |= (SCAN_MODE_ACTIVE << 24);
		MODE_ACTIVE(info->mode, settings[0]);

		setSystemResetedUp(0);
		break;

	default:
		logError(1,
			 "%s %s: invalid resume_bit value = %d! ERROR %08X\n",
			 tag, __func__, info->resume_bit, ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
	}

	logError(0, "%s %s: Mode Handler finished! res = %08X mode = %08X\n",
		 tag, __func__, res, info->mode);
	return res;
}

/**
  * Resume work function which perform a system reset, clean all the touches
  *from the linux input system and prepare the ground for enabling the sensing
  */
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_info *info;
	int r;
	info = container_of(work, struct fts_ts_info, resume_work);

	fts_disableInterrupt();

#ifdef CONFIG_PINCTRL
	if (info->ts_pinctrl) {
		r = pinctrl_select_state(info->ts_pinctrl,
					 info->pinctrl_state_active);
		if (r < 0)
			logError(
				1,
				"%s %s: Failed to select active pinstate, r:%d",
				tag, __func__, r);
	}
#endif

	info->resume_bit = 1;
	fts_system_reset();
	release_all_touches(info);
	fts_mode_handler(info, 0);
	info->sensor_sleep = false;

	fts_enableInterrupt();
}

/**
  * Suspend work function which clean all the touches from Linux input system
  *and prepare the ground to disabling the sensing
  */
static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_info *info;
	info = container_of(work, struct fts_ts_info, suspend_work);

	fts_disableInterrupt();
	info->resume_bit = 0;

#ifdef CONFIG_PINCTRL
	if (info->ts_pinctrl) {
		int r = pinctrl_select_state(info->ts_pinctrl,
					     info->pinctrl_state_suspend);
		if (r < 0)
			logError(
				1,
				"%s %s: Failed to select suspend pinstate, r:%d",
				tag, __func__, r);
	}
#endif
	fts_mode_handler(info, 0);
	release_all_touches(info);
	info->sensor_sleep = true;
}
/** @}*/

/**
  * From the name of the power regulator get/put the actual regulator structs
  * (copying their references into fts_ts_info variable)
  * @param info pointer to fts_ts_info which contains info about the device and
  * its hw setup
  * @param get if 1, the regulators are get otherwise they are put (released)
  * back to the system
  * @return OK if success or an error code which specify the type of error
  */
static int fts_get_reg(struct fts_ts_info *info, bool get)
{
	int retval;
	const struct fts_hw_platform_data *bdata = info->board;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->vdd_reg_name != NULL) && (*bdata->vdd_reg_name != 0)) {
		info->vdd_reg = regulator_get(info->dev, bdata->vdd_reg_name);
		if (IS_ERR(info->vdd_reg)) {
			logError(1, "%s %s: Failed to get power regulator\n",
				 tag, __func__);
			retval = PTR_ERR(info->vdd_reg);
			goto regulator_put;
		}
	}

	if ((bdata->avdd_reg_name != NULL) && (*bdata->avdd_reg_name != 0)) {
		info->avdd_reg = regulator_get(info->dev, bdata->avdd_reg_name);
		if (IS_ERR(info->avdd_reg)) {
			logError(1,
				 "%s %s: Failed to get bus pullup regulator\n",
				 tag, __func__);
			retval = PTR_ERR(info->avdd_reg);
			goto regulator_put;
		}
	}

	return OK;

regulator_put:
	if (info->vdd_reg) {
		regulator_put(info->vdd_reg);
		info->vdd_reg = NULL;
	}

	if (info->avdd_reg) {
		regulator_put(info->avdd_reg);
		info->avdd_reg = NULL;
	}

	return retval;
}

/**
  * Enable or disable the power regulators
  * @param info pointer to fts_ts_info which contains info about the device and
  * its hw setup
  * @param enable if 1, the power regulators are turned on otherwise they are
  * turned off
  * @return OK if success or an error code which specify the type of error
  */
static int fts_enable_reg(struct fts_ts_info *info, bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (info->vdd_reg) {
		retval = regulator_enable(info->vdd_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable bus regulator\n",
				 tag, __func__);
			goto exit;
		}
	}

	if (info->avdd_reg) {
		// Set required voltage first
		retval =
			regulator_set_voltage(info->avdd_reg, 3000000, 3000000);
		if (retval < 0) {
			logError(1, "%s %s Failed to set avdd reg voltage!\n",
				 tag, __func__);
			goto disable_bus_reg;
		}
		retval = regulator_enable(info->avdd_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable power regulator\n",
				 tag, __func__);
			goto disable_bus_reg;
		}
	}

	return OK;

disable_pwr_reg:
	if (info->avdd_reg)
		regulator_disable(info->avdd_reg);

disable_bus_reg:
	if (info->vdd_reg)
		regulator_disable(info->vdd_reg);

exit:
	return retval;
}

/**
  * Configure a GPIO according to the parameters
  * @param gpio gpio number
  * @param config if true, the gpio is set up otherwise it is free
  * @param dir direction of the gpio, 0 = in, 1 = out
  * @param state initial value (if the direction is in, this parameter is
  * ignored)
  * return error code
  */
static int fts_gpio_setup(int gpio, bool config, int dir, int state)
{
	int retval = 0;
	unsigned char buf[16];

	if (config) {
		snprintf(buf, 16, "fts_gpio_%u\n", gpio);

		retval = gpio_request(gpio, buf);
		if (retval) {
			logError(1, "%s %s: Failed to get gpio %d (code: %d)",
				 tag, __func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			logError(1, "%s %s: Failed to set gpio %d direction",
				 tag, __func__, gpio);
			return retval;
		}
	} else
		gpio_free(gpio);

	return retval;
}

/**
  * Setup the IRQ and RESET (if present) gpios.
  * If the Reset Gpio is present it will perform a cycle HIGH-LOW-HIGH in order
  *to assure that the IC has been reset properly
  */
static int fts_set_gpio(struct fts_ts_info *info)
{
	int retval;
	struct fts_hw_platform_data *bdata = info->board;

	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0);
	if (retval < 0) {
		logError(1, "%s %s: Failed to configure irq GPIO\n", tag,
			 __func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio, true, 1, 0);
		if (retval < 0) {
			logError(1, "%s %s: Failed to configure reset GPIO\n",
				 tag, __func__);
			goto err_gpio_reset;
		}
	}
	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, 0);
		msleep(10);
		gpio_set_value(bdata->reset_gpio, 1);
	}

	return OK;

err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
	bdata->reset_gpio = GPIO_NOT_DEFINED;
err_gpio_irq:
	return retval;
}

#ifdef CONFIG_PINCTRL
static int fts_pinctrl_init(struct fts_ts_info *info)
{
	int retval = 0;
	/* Get pinctrl if target uses pinctrl */
	info->ts_pinctrl = devm_pinctrl_get(info->dev);

	if (IS_ERR_OR_NULL(info->ts_pinctrl)) {
		retval = PTR_ERR(info->ts_pinctrl);
		dev_err(info->dev, "Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	info->pinctrl_state_active =
		pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_ACTIVE);

	if (IS_ERR_OR_NULL(info->pinctrl_state_active)) {
		retval = PTR_ERR(info->pinctrl_state_active);
		dev_err(info->dev, "Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	info->pinctrl_state_suspend =
		pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_SUSPEND);

	if (IS_ERR_OR_NULL(info->pinctrl_state_suspend)) {
		retval = PTR_ERR(info->pinctrl_state_suspend);
		dev_dbg(info->dev, "Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	return 0;
err_pinctrl_lookup:
	devm_pinctrl_put(info->ts_pinctrl);
err_pinctrl_get:
	info->ts_pinctrl = NULL;
	return retval;
}
#endif

/**
  * Retrieve and parse the hw information from the device tree node defined in
  * the system.
  * the most important information to obtain are: IRQ and RESET gpio numbers,
  * power regulator names
  * In the device file node is possible to define additional optional
  *information that can be parsed here.
  */
static int parse_dt(struct device *dev, struct fts_hw_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	bdata->irq_gpio = of_get_named_gpio(np, "fts,irq-gpio", 0);

	logError(0, "%s irq_gpio = %d\n", tag, bdata->irq_gpio);

	retval = of_property_read_string(np, "fts,regulator_dvdd", &name);
	if (retval == -EINVAL)
		bdata->vdd_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->vdd_reg_name = name;
		logError(0, "%s pwr_reg_name = %s\n", tag, name);
	}

	retval = of_property_read_string(np, "fts,regulator_avdd", &name);
	if (retval == -EINVAL)
		bdata->avdd_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->avdd_reg_name = name;
		logError(0, "%s bus_reg_name = %s\n", tag, name);
	}

	if (of_property_read_bool(np, "fts,reset-gpio")) {
		bdata->reset_gpio = of_get_named_gpio(np, "fts,reset-gpio", 0);
		logError(0, "%s reset_gpio =%d\n", tag, bdata->reset_gpio);
	} else
		bdata->reset_gpio = GPIO_NOT_DEFINED;

	retval = of_property_read_u32(np, "fts,irq-flags", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = temp_val;

	retval = of_property_read_u32(np, "fts,x-max", &temp_val);
	if (retval < 0)
		bdata->x_max = X_AXIS_MAX;
	else
		bdata->x_max = temp_val;

	retval = of_property_read_u32(np, "fts,y-max", &temp_val);
	if (retval < 0)
		bdata->y_max = Y_AXIS_MAX;
	else
		bdata->y_max = temp_val;

	return OK;
}

/**
  * Probe function, called when the driver it is matched with a device with the
  *same name compatible name
  * This function allocate, initialize and define all the most important
  *function and flow that are used by the driver to operate with the IC.
  * It allocates device variables, initialize queues and schedule works,
  *registers the IRQ handler, suspend/resume callbacks, registers the device to
  *the linux input subsystem etc.
  */
#ifdef I2C_INTERFACE
static int fts_probe(struct i2c_client *client)
{
#else
static int fts_probe(struct spi_device *client)
{
#endif

	struct fts_ts_info *info = NULL;
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval;
	int skip_5_1 = 0;
	u16 bus_type;

	logError(1, "%s %s: driver probe begin!\n", tag, __func__);

	logError(1, "%s driver ver. %s\n", tag, FTS_TS_DRV_VERSION);

	logError(1, "%s SET Bus Functionality :\n", tag);
#ifdef I2C_INTERFACE
	logError(1, "%s I2C interface...\n", tag);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		logError(1, "%s Unsupported I2C functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	logError(1, "%s i2c address: %x\n", tag, client->addr);
	bus_type = BUS_I2C;
#else
	logError(1, "%s SPI interface...\n", tag);
	client->mode = SPI_MODE_0;
#ifndef SPI4_WIRE
	client->mode |= SPI_3WIRE;
#endif
	client->max_speed_hz = SPI_CLOCK_FREQ;
	client->bits_per_word = 8;
	if (spi_setup(client) < 0) {
		logError(1, "%s Unsupported SPI functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}
	bus_type = BUS_SPI;
#endif

	logError(0, "%s SET Device driver INFO:\n", tag);

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		logError(
			1,
			"%s Out of memory... Impossible to allocate struct info!\n",
			tag);
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	info->client = client;
	info->dev = &info->client->dev;

	dev_set_drvdata(info->dev, info);

	if (dp) {
		info->board = devm_kzalloc(&client->dev,
					   sizeof(struct fts_hw_platform_data),
					   GFP_KERNEL);
		if (!info->board) {
			logError(1, "%s ERROR:info.board kzalloc failed\n",
				 tag);
			goto ProbeErrorExit_1;
		}
		parse_dt(&client->dev, info->board);
	}

	logError(0, "%s SET Regulators:\n", tag);
	retval = fts_get_reg(info, true);
	if (retval < 0) {
		logError(1, "%s ERROR: %s: Failed to get regulators\n", tag,
			 __func__);
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to enable regulators\n", tag,
			 __func__);
		goto ProbeErrorExit_2;
	}

	logError(0, "%s SET GPIOS:\n", tag);
	retval = fts_set_gpio(info);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to set up GPIO's\n", tag,
			 __func__);
		goto ProbeErrorExit_2;
	}

#ifdef CONFIG_PINCTRL
	logError(0, "%s %s: Pinctrl Init!\n", tag, __func__);
	error = fts_pinctrl_init(info);

	if (!error && info->ts_pinctrl) {
		error = pinctrl_select_state(info->ts_pinctrl,
					     info->pinctrl_state_active);
		if (error < 0) {
			logError(1, "%s %s: Failed to select %s pinstate %d\n",
				 tag, __func__, PINCTRL_STATE_ACTIVE, error);
		}
	} else {
		logError(1, "%s %s: Failed to init pinctrl\n", tag, __func__);
	}
#endif

	info->client->irq = gpio_to_irq(info->board->irq_gpio);

	logError(0, "%s SET Event Handler:\n", tag);
	device_init_wakeup(&client->dev, 1);
	info->event_wq =
		alloc_workqueue("fts-event-queue",
				WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!info->event_wq) {
		logError(1, "%s ERROR: Cannot create work thread\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_4;
	}

	INIT_WORK(&info->resume_work, fts_resume_work);
	INIT_WORK(&info->suspend_work, fts_suspend_work);

	info->dev = &info->client->dev;

	logError(0, "%s SET Input Device Property:\n", tag);
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		logError(1, "%s ERROR: No such input device defined!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}
	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
		 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = bus_type;
	info->input_dev->id.vendor = 0x0001;
	info->input_dev->id.product = 0x0002;
	info->input_dev->id.version = 0x0100;

	__set_bit(EV_SYN, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(BTN_TOUCH, info->input_dev->keybit);
	/* __set_bit(BTN_TOOL_FINGER, info->input_dev->keybit); */
	/* __set_bit(BTN_TOOL_PEN, info->input_dev->keybit); */

	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);

	/* input_mt_init_slots(info->input_dev, TOUCH_ID_MAX); */

	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X, X_AXIS_MIN,
			     info->board->x_max - 1, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y, Y_AXIS_MIN,
			     info->board->y_max - 1, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR, AREA_MIN,
			     AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR, AREA_MIN,
			     AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE, PRESSURE_MIN,
			     PRESSURE_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_DISTANCE, DISTANCE_MIN,
			     DISTANCE_MAX, 0, 0);

#ifdef PHONE_KEY
	/* KEY associated to the touch screen buttons */
	input_set_capability(info->input_dev, EV_KEY, KEY_HOMEPAGE);
	input_set_capability(info->input_dev, EV_KEY, KEY_BACK);
	input_set_capability(info->input_dev, EV_KEY, KEY_MENU);
#endif

	mutex_init(&(info->input_report_mutex));
	spin_lock_init(&fts_int);

	/* register the multi-touch input device */
	error = input_register_device(info->input_dev);
	if (error) {
		logError(1, "%s ERROR: No such input device\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5_1;
	}

	skip_5_1 = 1;
	/* track slots */
	info->touch_id = 0;
#ifdef STYLUS_MODE
	info->stylus_id = 0;
#endif

	/* init feature switches (by default all the features are disable,
	  * if one feature want to be enabled from the start,
	  * set the corresponding value to 1)*/
	info->glove_enabled = 0;
	info->charger_enabled = 0;
	info->cover_enabled = 0;
	info->grip_enabled = 0;

	info->resume_bit = 1;

	logError(0, "%s Init Core Lib:\n", tag);
	initCore(info);
	/* init hardware device */
	logError(1, "%s Device Initialization:\n", tag);
	error = fts_init(info);
	if (error < OK) {
		logError(1, "%s Cannot initialize the device ERROR %08X\n", tag,
			 error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	logError(1, "%s Sensing Initialization:\n", tag);
	error = fts_init_flow(info);
	if (error < OK) {
		logError(1, "%s Cannot execute init flow ERROR %08X\n", tag,
			 error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	logError(0, "%s SET Device File Nodes:\n", tag);
	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		logError(1, "%s ERROR: Cannot create sysfs structure!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	logError(1, "%s Probe Finished!\n", tag);
	return OK;

ProbeErrorExit_6:
	input_unregister_device(info->input_dev);

ProbeErrorExit_5_1:
	if (skip_5_1 != 1)
		input_free_device(info->input_dev);

ProbeErrorExit_5:
	destroy_workqueue(info->event_wq);

ProbeErrorExit_4:
	fts_enable_reg(info, false);

ProbeErrorExit_2:
	fts_get_reg(info, false);

ProbeErrorExit_1:
	kfree(info);

ProbeErrorExit_0:
	logError(1, "%s Probe Failed!\n", tag);

	return error;
}

/**
  * Clear and free all the resources associated to the driver.
  * This function is called when the driver need to be removed.
  */
#ifdef I2C_INTERFACE
static void fts_remove(struct i2c_client *client)
{
#else
static void fts_remove(struct spi_device *client)
{
#endif

	struct fts_ts_info *info = dev_get_drvdata(&(client->dev));

	/* sysfs stuff */
	sysfs_remove_group(&client->dev.kobj, &info->attrs);

	/* remove interrupt and event handlers */
	fts_interrupt_uninstall(info);

	/* unregister the device */
	input_unregister_device(info->input_dev);

	/* input_free_device(info->input_dev ); */

	/* Remove the work thread */
	destroy_workqueue(info->event_wq);

	fts_enable_reg(info, false);
	fts_get_reg(info, false);

	/* free all */
	kfree(info);
}

/**
  * Struct which contains the compatible names that need to match with
  * the definition of the device in the device tree node
  */
static struct of_device_id fts_of_match_table[] = {
	{
		.compatible = "st,fts",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fts_of_match_table);

#ifdef I2C_INTERFACE
static const struct i2c_device_id fts_device_id[] = { { FTS_TS_DRV_NAME, 0 },
						      {} };
MODULE_DEVICE_TABLE(i2c, fts_device_id);

static struct i2c_driver fts_i2c_driver = {
	.driver			= {
		.name		= FTS_TS_DRV_NAME,
		.owner 		= THIS_MODULE,
		.of_match_table = fts_of_match_table,
	},
	.probe			= fts_probe,
	.remove			= fts_remove,
	.id_table		= fts_device_id,
};
#else
static struct spi_driver fts_spi_driver = {
	.driver			= {
		.name		= FTS_TS_DRV_NAME,
		.of_match_table = fts_of_match_table,
		.owner		= THIS_MODULE,
	},
	.probe			= fts_probe,
	.remove			= fts_remove,
};
#endif

static int __init fts_driver_init(void)
{
#ifdef I2C_INTERFACE
	return i2c_add_driver(&fts_i2c_driver);
#else
	return spi_register_driver(&fts_spi_driver);
#endif
}

static void __exit fts_driver_exit(void)
{
#ifdef I2C_INTERFACE
	i2c_del_driver(&fts_i2c_driver);
#else
	spi_unregister_driver(&fts_spi_driver);
#endif
}

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

module_init(fts_driver_init);
module_exit(fts_driver_exit);
