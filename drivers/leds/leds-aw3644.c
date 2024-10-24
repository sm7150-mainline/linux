/*
 * Simple driver for Awinic AW3644 LED Flash driver chip
 * Copyright (C) 2017 Xiaomi Corp.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/pwm.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>

#define AW3644_NAME "leds-aw3644"

#define AW3644_IOC_MAGIC 'M'
#define AW3644_PRIVATE_NUM 100
#define AW3644_LED_NUMS 2

struct aw3644_platform_data {
	int tx_gpio;
	int torch_gpio;
	int hwen_gpio;
	int ito_detect_gpio;
	int ir_prot_time;
	unsigned int brightness;

	/* Simulative PWM settings */
	bool use_simulative_pwm;
	bool pass_mode;
	unsigned int pwm_period_us;
	unsigned int pwm_duty_us;
};

typedef struct {
	int ito_event;
} flood_report_data;

enum aw3644_event {
	GET_CHIP_ID_EVENT,
	SET_BRIGHTNESS_EVENT,
	GET_BRIGHTNESS_EVENT,
	MAX_NUM_EVENT,
};

typedef struct {
	unsigned int flood_enable;
	unsigned int flood_current;
	unsigned int flood_error;
} aw3644_info;

typedef struct {
	enum aw3644_event event;
	unsigned int data;
} aw3644_data;

#define FLOOD_IR_IOC_POWER_UP _IO(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 1)
#define FLOOD_IR_IOC_POWER_DOWN _IO(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 2)
#define FLOOD_IR_IOC_WRITE \
	_IOW(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 3, aw3644_data)
#define FLOOD_IR_IOC_READ \
	_IOWR(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 4, aw3644_data)
#define FLOOD_IR_IOC_READ_INFO \
	_IOWR(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 5, void *)

#define REG_ENABLE (0x1)
#define REG_IVFM_MODE (0x2)
#define REG_LED1_FLASH_BRIGHTNESS (0x3)
#define REG_LED2_FLASH_BRIGHTNESS (0x4)
#define REG_LED1_TORCH_BRIGHTNESS (0x5)
#define REG_LED2_TORCH_BRIGHTNESS (0x6)
#define REG_BOOST_CONF (0x7)
#define REG_TIMING_CONF (0x8)
#define REG_TEMP (0x9)
#define REG_FLAG1 (0xA)
#define REG_FLAG2 (0xB)
#define REG_DEVICE_ID (0xC)
#define REG_LAST_FLASH (0xD)
#define REG_MAX (0xD)

#define AW3644_ID (0x02)
#define AW3644TT_ID (0x04)

/* REG_ENABLE */
#define TX_PIN_ENABLE_SHIFT (7)
#define STROBE_TYPE_SHIFT (6)
#define STROBE_EN_SHIFT (5)
#define TORCH_PIN_ENABLE_SHIFT (4)
#define MODE_BITS_SHIFT (2)

#define STROBE_TYPE_LEVEL_TRIGGER (0)
#define STROBE_TYPE_EDGE_TRIGGER (1)

/* REG_LED1_BRIGHTNESS */
#define LED2_CURRENT_EQUAL (0x80)

#define AW3644_DEFAULT_PERIOD_US 2500000
#define AW3644_DEFAULT_DUTY_US 2500

#define NSECS_PER_USEC 1000UL

#define AW3644_MAX_BRIGHTNESS_VALUE 0x7F

/* REG_BOOST_CONF */
#define PASS_MODE_SHIFT (2)

#define DRV_NAME "flood"
#define AW3644_CLASS_NAME "aw3644"

static DECLARE_WAIT_QUEUE_HEAD(aw3644_poll_wait_queue);

enum aw3644_mode { MODES_STANDBY = 0, MODES_IR, MODES_TORCH, MODES_FLASH };

enum aw3644_pinctrl_state {
	STATE_ACTIVE = 0,
	STATE_ACTIVE_WITH_PWM,
	STATE_SUSPEND
};

struct aw3644_led {
	struct aw3644_chip_data *chip;
	struct led_classdev cdev;
	u32 num;
	u8 brightness;
};

struct aw3644_chip_data {
	struct device *dev;
	struct i2c_client *client;

	int num_leds;

	struct aw3644_led torch_leds[AW3644_LED_NUMS];
	struct led_classdev cdev_ir;
	struct cdev cdev;
	struct class *chr_class;
	struct device *chr_dev;

	dev_t dev_num;

	u8 br_ir;
	bool powerup_status;
	struct aw3644_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
	struct timer_list ir_stop_timer;
	struct work_struct ir_stop_work;

	unsigned int chip_id;
	unsigned int last_flag1;
	unsigned int last_flag2;
	int ito_irq;

	struct pwm_device *pwm;

	atomic_t ito_exception;

	/* pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_active_pwm; /* use simulative PWM */
	struct pinctrl_state *gpio_state_suspend;
};

/* chip power up */
static int aw3644_chip_powerup(struct aw3644_chip_data *chip, int value)
{
	int rc = -1;
	struct aw3644_platform_data *pdata;
	pdata = chip->pdata;
	if (gpio_is_valid(pdata->hwen_gpio)) {
		rc = gpio_direction_output(pdata->hwen_gpio, value);
		if (rc) {
			dev_err(chip->dev, "Unable to set hwen to output\n");
		}
	}
	return rc;
}
/* chip initialize */
static int aw3644_chip_init(struct aw3644_chip_data *chip)
{
	unsigned int chip_id;
	int ret;

	/* read device id */
	ret = regmap_read(chip->regmap, REG_DEVICE_ID, &chip_id);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_DEVICE_ID register\n");
		return ret;
	}

	if (chip_id != AW3644_ID && chip_id != AW3644TT_ID) {
		dev_err(chip->dev, "Invalid device id 0x%02X\n", chip_id);
		return -ENODEV;
	}

	chip->chip_id = chip_id;
	return ret;
}

static int aw3644_enable_pass_mode(struct aw3644_chip_data *chip)
{
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, REG_BOOST_CONF, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_BOOST_CONF register\n");
		return ret;
	}

	val |= (1 << PASS_MODE_SHIFT);
	ret = regmap_write(chip->regmap, REG_BOOST_CONF, val);
	if (ret < 0)
		dev_err(chip->dev, "Failed to write REG_BOOST_CONF register\n");

	return ret;
}

static bool aw3644_needs_suspend(struct aw3644_chip_data *chip)
{
	int i;
	bool ret = true;
	for (i = 0; i < AW3644_LED_NUMS; i++) {
		ret = chip->torch_leds[i].brightness == 0;
		if (!ret) {
			break;
		}
	}
	return ret;
}

/* chip control */
static int aw3644_control(struct aw3644_chip_data *chip, u8 brightness,
			  enum aw3644_mode opmode)
{
	int ret = -1, val, i;
	struct aw3644_led *led;
	if (chip->powerup_status == false) {
		dev_err(chip->dev, "device not power up\n");
		goto out;
	}
	ret = regmap_read(chip->regmap, REG_FLAG1, &chip->last_flag1);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_FLAG1 Register\n");
		goto out;
	}

	ret = regmap_read(chip->regmap, REG_FLAG2, &chip->last_flag2);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read REG_FLAG2 Register\n");
		goto out;
	}

	if (chip->last_flag1 || chip->last_flag2)
		dev_info(chip->dev, "Last FLAG1 is 0x%02X, FLAG2 is 0x%02X\n",
			 chip->last_flag1, chip->last_flag2);
	dev_dbg(chip->dev, "[%s]: brightness = %u, opmode = %d\n", __func__,
		brightness, opmode);

	/* brightness 0 means off-state */
	if (aw3644_needs_suspend(chip))
		opmode = MODES_STANDBY;

	if (opmode == MODES_FLASH) {
		dev_err(chip->dev, "Flash mode not supported\n");
		opmode = MODES_STANDBY;
	}

	if (opmode != MODES_IR) {
		if (chip->pdata->use_simulative_pwm && chip->pwm != NULL) {
			pwm_disable(chip->pwm);
			dev_dbg(chip->dev, "Simulative PWM disabled\n");
		}

		cancel_work(&chip->ir_stop_work);
		del_timer(&chip->ir_stop_timer);
	}

	if (opmode != MODES_STANDBY) {
		val = (opmode << MODE_BITS_SHIFT);
		for (i = 0; i < AW3644_LED_NUMS; i++) {
			led = &chip->torch_leds[i];
			val |= (led->brightness != 0) << led->num;
		}
	}

	switch (opmode) {
	case MODES_TORCH:
		if (gpio_is_valid(chip->pdata->torch_gpio))
			val |= 1 << TORCH_PIN_ENABLE_SHIFT;

		ret = regmap_write(chip->regmap, REG_ENABLE, val);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_ENABLE register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED1_TORCH_BRIGHTNESS,
				   brightness | LED2_CURRENT_EQUAL);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_LED1_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED2_TORCH_BRIGHTNESS,
				   brightness);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_LED2_TORCH_BRIGHTNESS register\n");
			goto out;
		}
		break;

	case MODES_IR:
		/* Enable STORBE_EN bit */
		val |= 1 << STROBE_EN_SHIFT;

		ret = regmap_write(chip->regmap, REG_ENABLE, val);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_ENABLE register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED1_FLASH_BRIGHTNESS,
				   brightness | LED2_CURRENT_EQUAL);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_LED1_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		ret = regmap_write(chip->regmap, REG_LED2_FLASH_BRIGHTNESS,
				   brightness);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_LED2_TORCH_BRIGHTNESS register\n");
			goto out;
		}

		if (chip->pdata->use_simulative_pwm && chip->pwm != NULL) {
			ret = pwm_enable(chip->pwm);
			if (ret < 0) {
				dev_err(chip->dev,
					"Failed to enable PWM device\n");
				goto out;

			} else
				dev_err(chip->dev, "Simulative PWM enabled\n");
		}

		if (chip->pdata->ir_prot_time > 0)
			mod_timer(&chip->ir_stop_timer,
				  jiffies + msecs_to_jiffies(
						    chip->pdata->ir_prot_time));
		break;

	case MODES_STANDBY:
		ret = regmap_write(chip->regmap, REG_ENABLE, 0);
		if (ret < 0) {
			dev_err(chip->dev,
				"Failed to write REG_ENABLE register\n");
			goto out;
		}
		break;

	default:
		return ret;
	}

out:
	return ret;
}

/* torch mode */
static int aw3644_torch_brightness_set(struct led_classdev *cdev,
				       enum led_brightness brightness)
{
	struct aw3644_led *led = container_of(cdev, struct aw3644_led, cdev);
	int ret = -1;

	mutex_lock(&led->chip->lock);
	if (led->chip->powerup_status == false) {
		ret = aw3644_chip_powerup(led->chip, 1);
		if (ret == 0)
			led->chip->powerup_status = true;
	}

	led->brightness = brightness;
	ret = aw3644_control(led->chip, brightness, MODES_TORCH);

	if (aw3644_needs_suspend(led->chip)) {
		ret = aw3644_chip_powerup(led->chip, 0);
		if (ret == 0)
			led->chip->powerup_status = false;
	}
	mutex_unlock(&led->chip->lock);
	return ret;
}

/* ir mode */
static int aw3644_ir_brightness_set(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct aw3644_chip_data *chip =
		container_of(cdev, struct aw3644_chip_data, cdev_ir);
	int ret;

	mutex_lock(&chip->lock);
	chip->br_ir = brightness;
	ret = aw3644_control(chip, chip->br_ir, MODES_IR);
	mutex_unlock(&chip->lock);

	return ret;
}

static enum led_brightness aw3644_ir_brightness_get(struct led_classdev *cdev)
{
	struct aw3644_chip_data *chip =
		container_of(cdev, struct aw3644_chip_data, cdev_ir);

	return chip->br_ir;
}

static int aw3644_init_pinctrl(struct aw3644_chip_data *chip)
{
	struct device *dev = chip->dev;

	chip->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		dev_err(dev, "Unable to acquire pinctrl\n");
		chip->pinctrl = NULL;
		return 0;
	}

	chip->gpio_state_active =
		pinctrl_lookup_state(chip->pinctrl, "aw3644_led_active");
	if (IS_ERR_OR_NULL(chip->gpio_state_active)) {
		dev_err(dev, "Cannot lookup LED active state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_active);
	}

	chip->gpio_state_active_pwm =
		pinctrl_lookup_state(chip->pinctrl, "aw3644_led_active_pwm");
	if (IS_ERR_OR_NULL(chip->gpio_state_active_pwm)) {
		dev_err(dev,
			"Cannot lookup LED active with simulative PWM state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_active_pwm);
	}

	chip->gpio_state_suspend =
		pinctrl_lookup_state(chip->pinctrl, "aw3644_led_suspend");
	if (IS_ERR_OR_NULL(chip->gpio_state_suspend)) {
		dev_err(dev, "Cannot lookup LED suspend state\n");
		devm_pinctrl_put(chip->pinctrl);
		chip->pinctrl = NULL;
		return PTR_ERR(chip->gpio_state_suspend);
	}

	return 0;
}

static int aw3644_pinctrl_select(struct aw3644_chip_data *chip,
				 enum aw3644_pinctrl_state state)
{
	int ret = 0;
	struct pinctrl_state *pins_state;
	struct device *dev = chip->dev;

	switch (state) {
	case STATE_ACTIVE:
		pins_state = chip->gpio_state_active;
		break;
	case STATE_ACTIVE_WITH_PWM:
		pins_state = chip->gpio_state_active_pwm;
		break;
	case STATE_SUSPEND:
		pins_state = chip->gpio_state_suspend;
		break;
	default:
		dev_err(chip->dev, "Invalid pinctrl state %d\n", state);
		return -ENODEV;
	}

	ret = pinctrl_select_state(chip->pinctrl, pins_state);
	if (ret < 0)
		dev_err(dev, "Failed to select pins state %d\n", state);

	return ret;
}

static int aw3644_ir_release(struct inode *node, struct file *filp)
{
	struct aw3644_chip_data *chip =
		container_of(node->i_cdev, struct aw3644_chip_data, cdev);

	disable_irq_nosync(chip->ito_irq);

	return 0;
}

static void aw3644_ir_stop_work(struct work_struct *work)
{
	struct aw3644_chip_data *chip =
		container_of(work, struct aw3644_chip_data, ir_stop_work);

	aw3644_ir_brightness_set(&chip->cdev_ir, LED_OFF);
}

static void aw3644_ir_stop_timer(struct timer_list *t)
{
	struct aw3644_chip_data *chip = from_timer(chip, t, ir_stop_timer);

	dev_err(chip->dev, "Force shutdown IR LED after %d msecs\n",
		chip->pdata->ir_prot_time);
	schedule_work(&chip->ir_stop_work);
}

static irqreturn_t aw3644_ito_irq(int ic_irq, void *dev_id)
{
	struct aw3644_chip_data *chip = dev_id;

	dev_err(chip->dev, "ITO EXCEPTION!\n");
	atomic_set(&chip->ito_exception, 1);

	wake_up(&aw3644_poll_wait_queue);
	return IRQ_HANDLED;
}

static int aw3644_ir_open(struct inode *node, struct file *filp)
{
	struct aw3644_chip_data *chip =
		container_of(node->i_cdev, struct aw3644_chip_data, cdev);

	filp->private_data = chip;
	enable_irq(chip->ito_irq);
	return 0;
}

static ssize_t aw3644_ir_write(struct file *filp, const char *buf, size_t len,
			       loff_t *fseek)
{
	int ret = 0;

	return ret;
}

static ssize_t aw3644_ir_read(struct file *filp, char *buf, size_t len,
			      loff_t *fseek)
{
	int ret = 0;
	int data = 0;
	struct aw3644_chip_data *chip = filp->private_data;

	data = atomic_read(&chip->ito_exception);
	if (copy_to_user(buf, &data, sizeof(data)) != 0)
		dev_err(chip->dev, "copy to user failed!\n");

	return ret;
}

static unsigned int aw3644_poll(struct file *filp, poll_table *wait)
{
	int ret = 0;
	unsigned int mask = 0;
	struct aw3644_chip_data *chip = filp->private_data;

	dev_dbg(chip->dev, "Poll enter\n");

	poll_wait(filp, &aw3644_poll_wait_queue, wait);
	if (atomic_read(&chip->ito_exception)) {
		mask = POLLIN | POLLRDNORM;

		mutex_lock(&chip->lock);
		ret = aw3644_control(chip, LED_OFF, MODES_STANDBY);
		mutex_unlock(&chip->lock);

		aw3644_ir_stop_timer(&chip->ir_stop_timer);

		if (gpio_is_valid(chip->pdata->hwen_gpio)) {
			ret = gpio_direction_output(chip->pdata->hwen_gpio, 0);
			if (ret)
				dev_err(chip->dev,
					"Unable to shutdown flood\n");
		}
	}

	return mask;
}

static int aw3644_ir_init(struct aw3644_chip_data *chip)
{
	int ret = 0;

	ret = aw3644_control(chip, LED_OFF, MODES_IR);
	if (ret < 0)
		dev_err(chip->dev, "Init failed, %d\n", ret);

	return ret;
}

static int aw3644_ir_deinit(struct aw3644_chip_data *chip)
{
	int ret = 0;

	ret = aw3644_control(chip, LED_OFF, MODES_STANDBY);
	if (ret < 0)
		dev_err(chip->dev, "Deinit failed, %d\n", ret);

	return ret;
}

static int aw3644_ir_set_data(struct aw3644_chip_data *chip, aw3644_data params)
{
	int ret = 0;
	uint8_t brightness;

	if (SET_BRIGHTNESS_EVENT == params.event) {
		brightness = (uint8_t)params.data;
		if (brightness > 0x3F) {
			dev_err(chip->dev,
				"brightness %d is higher then the max value 0x3F, set to 0x3F\n",
				brightness);
			brightness = 0x3F;
		}
		ret = aw3644_control(chip, brightness, MODES_IR);
		if (ret < 0)
			dev_err(chip->dev, "Set brightness failed, %d\n", ret);
	}

	return ret;
}

static int aw3644_ir_get_data(struct aw3644_chip_data *chip, aw3644_data params)
{
	int ret = 0;
	unsigned int data = 0;

	if (GET_CHIP_ID_EVENT == params.event)
		data = chip->chip_id;
	if (GET_BRIGHTNESS_EVENT == params.event)
		data = chip->pdata->brightness;

	if (copy_to_user((void __user *)&params.data, &data, sizeof(data))) {
		ret = -ENODEV;
		dev_err(chip->dev, "Copy data to user space failed\n");
	}

	return ret;
}

static long aw3644_ir_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct aw3644_chip_data *chip = filp->private_data;
	aw3644_data params;

	dev_dbg(chip->dev, "[%s] ioctl_cmd = %u\n", __func__, cmd);

	if (_IOC_TYPE(cmd) != AW3644_IOC_MAGIC)
		return -ENODEV;

	ret = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	if ((_IOC_DIR(cmd) & _IOC_WRITE) ||
	    (_IOC_DIR(cmd) & (_IOC_WRITE | _IOC_READ))) {
		if (copy_from_user(&params, (void __user *)arg,
				   sizeof(aw3644_data))) {
			dev_err(chip->dev,
				"Copy data from user space failed\n");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case FLOOD_IR_IOC_POWER_UP:
		ret = aw3644_ir_init(chip);
		break;
	case FLOOD_IR_IOC_POWER_DOWN:
		ret = aw3644_ir_deinit(chip);
		break;
	case FLOOD_IR_IOC_WRITE:
		ret = aw3644_ir_set_data(chip, params);
		break;
	case FLOOD_IR_IOC_READ:
		ret = aw3644_ir_get_data(chip, params);
		break;
	case FLOOD_IR_IOC_READ_INFO: {
		aw3644_info flood_info;
		unsigned int ret = 0;
		unsigned int val;

		ret += regmap_read(chip->regmap, REG_ENABLE, &val);
		flood_info.flood_enable = (val == 0x27) ? 1 : 0;

		ret += regmap_read(chip->regmap, REG_LED2_FLASH_BRIGHTNESS,
				   &val);
		flood_info.flood_current = (val * 11725 + 10900) * 2 / 1000;

		ret += regmap_read(chip->regmap, REG_FLAG1, &val);
		flood_info.flood_error = val;
		ret += regmap_read(chip->regmap, REG_FLAG2, &val);
		flood_info.flood_error = (flood_info.flood_error << 8) + val;

		if (copy_to_user((void __user *)arg, &flood_info,
				 sizeof(flood_info)) != 0 ||
		    ret != 0)
			dev_err(chip->dev, "copy to user failed!\n");

		dev_err(chip->dev, "flood_info:en=%d, current=%d, error=0x%x\n",
			flood_info.flood_enable, flood_info.flood_current,
			flood_info.flood_error);
		break;
	}
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long aw3644_ir_compat_ioctl(struct file *filp, unsigned int cmd,
				   unsigned long arg)
{
	return aw3644_ir_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t aw3644_pwm_period_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);
	int ret;
	unsigned int val;

	ret = kstrtou32(buff, 10, &val);
	if (ret < 0)
		return ret;

	chip->pdata->pwm_period_us = val;

	mutex_lock(&chip->lock);
	ret = pwm_config(chip->pwm, chip->pdata->pwm_duty_us * NSECS_PER_USEC,
			 chip->pdata->pwm_period_us * NSECS_PER_USEC);
	if (ret < 0) {
		dev_err(chip->dev, "PWM config failed: %d\n", ret);
		mutex_unlock(&chip->lock);
		return ret;
	}
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t aw3644_pwm_period_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->pwm_period_us);
}

static ssize_t aw3644_pwm_duty_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);
	int ret;
	unsigned int val;

	ret = kstrtou32(buff, 10, &val);
	if (ret < 0)
		return ret;

	chip->pdata->pwm_duty_us = val;

	mutex_lock(&chip->lock);
	ret = pwm_config(chip->pwm, chip->pdata->pwm_duty_us * NSECS_PER_USEC,
			 chip->pdata->pwm_period_us * NSECS_PER_USEC);
	if (ret < 0) {
		dev_err(chip->dev, "PWM config failed: %d\n", ret);
		mutex_unlock(&chip->lock);
		return ret;
	}
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t aw3644_pwm_duty_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", chip->pdata->pwm_duty_us);
}

static int aw3644_dump_reg(struct device *dev)
{
	unsigned int val;
	int ret = 0;
	int i = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);

	dev_err(dev, "aw3644_dump_reg start:\n");
	for (i = 0; i < 13; i++) {
		ret += regmap_read(chip->regmap, i + 1, &val);
		dev_err(dev, "aw3644 0x%x:0x%x", i + 1, val);
	}

	if (ret != 0)
		dev_err(dev, "lm reg dump fail!\n");
	else
		dev_err(dev, "lm reg dump success!\n");

	return 0;
}

static ssize_t aw3644_reg_opt_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buff, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);
	unsigned int val, addr;
	int ret = 0;

	ret = sscanf(buff, "0x%x 0x%x", &addr, &val);
	if (ret == 0)
		dev_err(chip->dev, "aw3644_reg_opt_store, reg=0x%x,val=0x%x.\n",
			addr, val);

	if (addr > 0x13) {
		dev_err(chip->dev, "aw3644_reg_opt_store, addr invalid:0x%x\n",
			addr);
		return count;
	}

	ret = regmap_write(chip->regmap, addr, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"Failed to write reg:0x%x, val:0x%x, ret:%d\n", addr,
			val, ret);
		return ret;
	}
	regmap_read(chip->regmap, addr, &val);
	dev_err(chip->dev,
		"aw3644_reg_opt_store, reg:0x%x, val_after_set:0x%x.\n", addr,
		val);

	return count;
}

static ssize_t aw3644_reg_opt_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);

	aw3644_dump_reg(dev);

	dev_err(chip->dev, "aw3644_reg_opt_show\n");
	return snprintf(buf, PAGE_SIZE, "aw3644_reg_opt_show\n");
}

static ssize_t aw3644_id_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw3644_chip_data *chip =
		container_of(led_cdev, struct aw3644_chip_data, cdev_ir);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(chip->chip_id == AW3644_ID) ? "AW3644" : "AW3644TT");
}

static DEVICE_ATTR(pwm_period, S_IRUGO | S_IWUSR, aw3644_pwm_period_show,
		   aw3644_pwm_period_store);
static DEVICE_ATTR(pwm_duty, S_IRUGO | S_IWUSR, aw3644_pwm_duty_show,
		   aw3644_pwm_duty_store);
static DEVICE_ATTR(id, S_IRUGO, aw3644_id_show, NULL);
static DEVICE_ATTR(reg_opt, S_IRUGO | S_IWUSR, aw3644_reg_opt_show,
		   aw3644_reg_opt_store);

static struct attribute *aw3644_ir_attrs[] = { &dev_attr_pwm_period.attr,
					       &dev_attr_pwm_duty.attr,
					       &dev_attr_id.attr,
					       &dev_attr_reg_opt.attr, NULL };
ATTRIBUTE_GROUPS(aw3644_ir);

static struct aw3644_platform_data *aw3644_parse_dt(struct i2c_client *client)
{
	struct aw3644_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	int ret = 0;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->hwen_gpio = of_get_named_gpio(np, "aw3644,hwen-gpio", 0);
	pdata->torch_gpio = of_get_named_gpio(np, "aw3644,torch-gpio", 0);
	pdata->tx_gpio = of_get_named_gpio(np, "aw3644,tx-gpio", 0);
	pdata->ito_detect_gpio =
		of_get_named_gpio(np, "aw3644,ito-detect-gpio", 0);

	pdata->pass_mode = of_property_read_bool(np, "aw3644,pass-mode");

	pdata->use_simulative_pwm =
		of_property_read_bool(np, "aw3644,use-simulative-pwm");

	ret = of_property_read_s32(np, "aw3644,ir-prot-time",
				   &pdata->ir_prot_time);
	if (ret < 0) {
		dev_info(&client->dev,
			 "No protect time specified for IR mode\n");
		pdata->ir_prot_time = -1;
	}

	if (pdata->use_simulative_pwm) {
		ret = of_property_read_u32(np, "aw3644,period-us",
					   &pdata->pwm_period_us);
		if (ret < 0) {
			dev_err(&client->dev,
				"Could not find PWM period, use default value\n");
			pdata->pwm_period_us = AW3644_DEFAULT_PERIOD_US;
		}

		ret = of_property_read_u32(np, "aw3644,duty-us",
					   &pdata->pwm_duty_us);
		if (ret < 0) {
			dev_err(&client->dev,
				"Could not find PWM duty, use default value\n");
			pdata->pwm_period_us = AW3644_DEFAULT_DUTY_US;
		}
	}

	return pdata;
}

static int aw3644_parse_leds(struct aw3644_chip_data *chip)
{
	struct device_node *np = chip->client->dev.of_node, *child;
	int count, ret = 0, i = 0;
	struct aw3644_led *led;

	count = of_get_available_child_count(np);
	if (!count || count > AW3644_LED_NUMS)
		return -EINVAL;

	for_each_available_child_of_node(np, child) {
		struct led_init_data init_data = {};
		u32 source;

		ret = of_property_read_u32(child, "reg", &source);
		if (ret != 0 || source >= AW3644_LED_NUMS) {
			dev_err(&chip->client->dev,
				"Couldn't read LED address: %d\n", ret);
			count--;
			continue;
		}

		led = &chip->torch_leds[i];
		led->num = source;
		led->chip = chip;
		led->brightness = 0;
		led->cdev.max_brightness = AW3644_MAX_BRIGHTNESS_VALUE;
		led->cdev.brightness_set_blocking = aw3644_torch_brightness_set;
		init_data.fwnode = of_fwnode_handle(child);

		ret = devm_led_classdev_register_ext(&chip->client->dev,
						     &led->cdev, &init_data);
		if (ret < 0) {
			of_node_put(child);
			return ret;
		}

		i++;
	}

	if (!count)
		return -EINVAL;

	chip->num_leds = i;

	return 0;
}

static const struct regmap_config aw3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static const struct file_operations aw3644_ir_fops = {
	.owner = THIS_MODULE,
	.release = aw3644_ir_release,
	.open = aw3644_ir_open,
	.read = aw3644_ir_read,
	.write = aw3644_ir_write,
	.unlocked_ioctl = aw3644_ir_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aw3644_ir_compat_ioctl,
#endif
	.poll = aw3644_poll
};

static int aw3644_probe(struct i2c_client *client)
{
	struct aw3644_platform_data *pdata;
	struct aw3644_chip_data *chip;
	enum aw3644_pinctrl_state pin_state = STATE_ACTIVE;

	int err;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	chip = devm_kzalloc(&client->dev, sizeof(struct aw3644_chip_data),
			    GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	if (client->dev.of_node) {
		pdata = aw3644_parse_dt(client);
		if (IS_ERR(pdata)) {
			dev_err(&client->dev, "Failed to parse devicetree\n");
			return -ENODEV;
		}
	} else
		pdata = dev_get_platdata(&client->dev);

	if (pdata == NULL) {
		dev_err(&client->dev, "needs platform Data.\n");
		return -ENODATA;
	}

	chip->dev = &client->dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(client, &aw3644_regmap);
	if (IS_ERR(chip->regmap)) {
		err = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			err);
		return err;
	}

	/* Simulative PWM output */
	if (pdata->use_simulative_pwm) {
		chip->pwm = pwm_get(&client->dev, NULL);
		if (IS_ERR(chip->pwm)) {
			err = PTR_ERR(chip->pwm);
			dev_err(&client->dev, "Failed to get PWM device: %d\n",
				err);
			chip->pwm = NULL;
		}

		err = pwm_config(chip->pwm, pdata->pwm_duty_us * NSECS_PER_USEC,
				 pdata->pwm_period_us * NSECS_PER_USEC);
		if (err < 0) {
			dev_err(&client->dev, "PWM config failed: %d\n", err);
			goto err_free_pwm;
		}

		pin_state = STATE_ACTIVE_WITH_PWM;
	}

	err = aw3644_init_pinctrl(chip);
	if (err) {
		dev_err(&client->dev, "Failed to initialize pinctrl\n");
		goto err_free_pwm;
	} else {
		if (chip->pinctrl) {
			/* Target support pinctrl */
			err = aw3644_pinctrl_select(chip, pin_state);
			if (err) {
				dev_err(&client->dev,
					"Failed to select pinctrl\n");
				goto err_free_pwm;
			}
		}
	}

	if (gpio_is_valid(pdata->hwen_gpio)) {
		/*
		err = gpio_request(pdata->hwen_gpio, "aw3644_hwen");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
					pdata->hwen_gpio);
			goto err_pinctrl_sleep;
		}
	*/
		err = gpio_direction_output(pdata->hwen_gpio, 1);
		if (err) {
			dev_err(&client->dev, "Unable to set hwen to output\n");
			goto err_pinctrl_sleep;
		}
		msleep(10);
	}

	if (gpio_is_valid(pdata->tx_gpio)) {
		err = gpio_request(pdata->tx_gpio, "aw3644_tx");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
				pdata->tx_gpio);
			goto err_free_hwen_gpio;
		}

		err = gpio_direction_output(pdata->tx_gpio, 0);
		if (err) {
			dev_err(&client->dev,
				"Unable to set tx_gpio to output\n");
			goto err_free_hwen_gpio;
		}
	}

	if (gpio_is_valid(pdata->torch_gpio)) {
		err = gpio_request(pdata->torch_gpio, "aw3644_torch");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
				pdata->torch_gpio);
			goto err_free_tx_gpio;
		}

		err = gpio_direction_output(pdata->torch_gpio, 0);
		if (err) {
			dev_err(&client->dev,
				"Unable to set torch_gpio to output\n");
			goto err_free_hwen_gpio;
		}
	}

	if (gpio_is_valid(pdata->ito_detect_gpio)) {
		err = gpio_request(pdata->ito_detect_gpio, "aw3644_ito_det");
		if (err) {
			dev_err(&client->dev, "Unable to request gpio[%d]\n",
				pdata->ito_detect_gpio);
			goto err_free_torch_gpio;
		}

		err = gpio_direction_input(pdata->ito_detect_gpio);
		if (err) {
			dev_err(&client->dev,
				"Unable to set ito_detect to input\n");
			goto err_free_ito_gpio;
		}

		chip->ito_irq = gpio_to_irq(pdata->ito_detect_gpio);
		err = request_threaded_irq(chip->ito_irq, NULL, aw3644_ito_irq,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "aw3644_ito_det", chip);
		if (err < 0) {
			dev_err(&client->dev, "Unable to request irq\n");
			goto err_free_ito_gpio;
		}
	}
	disable_irq_nosync(chip->ito_irq);

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	INIT_WORK(&chip->ir_stop_work, aw3644_ir_stop_work);
	timer_setup(&chip->ir_stop_timer, aw3644_ir_stop_timer, 0);

	err = aw3644_chip_init(chip);
	if (err < 0)
		goto err_free_ito_irq;

	if (pdata->pass_mode) {
		err = aw3644_enable_pass_mode(chip);
		if (err < 0)
			goto err_free_ito_irq;
	}

	chip->chr_class = class_create(AW3644_CLASS_NAME);
	if (chip->chr_class == NULL) {
		dev_err(&client->dev, "Failed to create class.\n");
		err = -ENODEV;
		goto err_free_ito_irq;
	}

	err = alloc_chrdev_region(&chip->dev_num, 0, 1, DRV_NAME);
	if (err < 0) {
		dev_err(&client->dev, "Failed to allocate chrdev region\n");
		goto err_destory_class;
	}

	chip->chr_dev = device_create(chip->chr_class, NULL, chip->dev_num,
				      chip, DRV_NAME);
	if (IS_ERR(chip->chr_dev)) {
		dev_err(&client->dev, "Failed to create char device\n");
		err = PTR_ERR(chip->chr_dev);
		goto err_unregister_chrdev;
	}

	cdev_init(&(chip->cdev), &aw3644_ir_fops);
	chip->cdev.owner = THIS_MODULE;

	err = cdev_add(&(chip->cdev), chip->dev_num, 1);
	if (err < 0) {
		dev_err(&client->dev, "Failed to add cdev\n");
		goto err_destroy_device;
	}

	/* torch mode */
	err = aw3644_parse_leds(chip);
	if (err < 0) {
		dev_err(chip->dev, "Failed to register torch LEDs\n");
		goto err_del_cdev;
	}
	/* ir mode */
	chip->cdev_ir.name = "ir";
	chip->cdev_ir.max_brightness = AW3644_MAX_BRIGHTNESS_VALUE;
	chip->cdev_ir.brightness_set_blocking = aw3644_ir_brightness_set;
	chip->cdev_ir.brightness_get = aw3644_ir_brightness_get;
	if (pdata->use_simulative_pwm)
		chip->cdev_ir.groups = aw3644_ir_groups;
	err = led_classdev_register((struct device *)&client->dev,
				    &chip->cdev_ir);
	if (err < 0) {
		dev_err(chip->dev, "Failed to register ir\n");
		goto err_del_cdev;
	}

	atomic_set(&chip->ito_exception, 0);

	dev_info(&client->dev, "Exit\n");

	return 0;

err_del_cdev:
	cdev_del(&(chip->cdev));
err_destroy_device:
	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);
err_unregister_chrdev:
	unregister_chrdev_region(chip->dev_num, 1);
err_destory_class:
	if (chip->chr_class)
		class_destroy(chip->chr_class);

err_free_ito_irq:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		free_irq(chip->ito_irq, chip);
err_free_ito_gpio:
	if (gpio_is_valid(pdata->ito_detect_gpio))
		gpio_free(pdata->ito_detect_gpio);

err_free_torch_gpio:
	if (gpio_is_valid(pdata->torch_gpio)) {
		gpio_set_value(pdata->torch_gpio, 0);
		gpio_free(pdata->torch_gpio);
	}
err_free_tx_gpio:
	if (gpio_is_valid(pdata->tx_gpio)) {
		gpio_set_value(pdata->tx_gpio, 0);
		gpio_free(pdata->tx_gpio);
	}
err_free_hwen_gpio:
	if (gpio_is_valid(pdata->hwen_gpio)) {
		/* Pull HWEN to ground to reduce power */
		gpio_set_value(pdata->hwen_gpio, 0);
		gpio_free(pdata->hwen_gpio);
	}

err_pinctrl_sleep:
	if (chip->pinctrl) {
		if (aw3644_pinctrl_select(chip, STATE_SUSPEND) < 0)
			dev_err(&client->dev,
				"Failed to select suspend pinstate\n");
		devm_pinctrl_put(chip->pinctrl);
	}

err_free_pwm:
	pwm_put(chip->pwm);

	return err;
}

static void aw3644_remove(struct i2c_client *client)
{
	struct aw3644_chip_data *chip = i2c_get_clientdata(client);
	cdev_del(&(chip->cdev));

	if (chip->chr_dev)
		device_destroy(chip->chr_class, chip->dev_num);

	unregister_chrdev_region(chip->dev_num, 1);

	if (chip->chr_dev)
		class_destroy(chip->chr_class);

	cancel_work(&chip->ir_stop_work);
	del_timer(&chip->ir_stop_timer);
	led_classdev_unregister(&chip->cdev_ir);
	regmap_write(chip->regmap, REG_ENABLE, 0);
	pwm_put(chip->pwm);

	if (gpio_is_valid(chip->pdata->hwen_gpio)) {
		gpio_set_value(chip->pdata->hwen_gpio, 0);
		gpio_free(chip->pdata->hwen_gpio);
	}

	if (gpio_is_valid(chip->pdata->tx_gpio)) {
		gpio_set_value(chip->pdata->tx_gpio, 0);
		gpio_free(chip->pdata->tx_gpio);
	}

	if (gpio_is_valid(chip->pdata->torch_gpio)) {
		gpio_set_value(chip->pdata->torch_gpio, 0);
		gpio_free(chip->pdata->torch_gpio);
	}

	if (gpio_is_valid(chip->pdata->ito_detect_gpio)) {
		free_irq(chip->ito_irq, chip);
		gpio_free(chip->pdata->ito_detect_gpio);
	}
	if (chip->pinctrl) {
		aw3644_pinctrl_select(chip, STATE_SUSPEND);
	}
}

static const struct of_device_id aw3644_match_table[] = {
	{
		.compatible = "awinic,aw3644",
	},
	{},
};

MODULE_DEVICE_TABLE(of, aw3644_match_table);

static const struct i2c_device_id aw3644_id[] = { { AW3644_NAME, 0 }, {} };

MODULE_DEVICE_TABLE(i2c, aw3644_id);

static struct i2c_driver aw3644_i2c_driver = {
	.driver = {
		.name = AW3644_NAME,
		.of_match_table = aw3644_match_table,
	},
	.probe = aw3644_probe,
	.remove = aw3644_remove,
	.id_table = aw3644_id,
};

module_i2c_driver(aw3644_i2c_driver);

MODULE_DESCRIPTION("Awinic Flash Lighting driver for AW3644");
MODULE_AUTHOR("Tao, Jun <taojun@xiaomi.com>");
MODULE_LICENSE("GPL v2");
