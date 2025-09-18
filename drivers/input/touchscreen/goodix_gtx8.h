/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __GOODIX_GTX8_H__
#define __GOODIX_GTX8_H__

#define GOODIX_GTX8_NORMAL_RESET_DELAY_MS	100
#define GOODIX_GTX8_POWER_ON_DELAY_MS		300

#define GOODIX_GTX8_TOUCH_EVENT			BIT(7)
#define GOODIX_GTX8_REQUEST_EVENT		BIT(6)
#define GOODIX_GTX8_TOUCH_COUNT_MASK		GENMASK(3, 0)
#define GOODIX_GTX8_FINGER_ID_MASK_YELLOWSTONE	GENMASK(7, 4)

#define GOODIX_GTX8_MAX_TOUCH			10
#define GOODIX_GTX8_CHECKSUM_SIZE		sizeof(u16)

#define GOODIX_GTX8_FW_VERSION_ADDR_NORMANDY	0x4535
#define GOODIX_GTX8_FW_VERSION_ADDR_YELLOWSTONE	0x4022
#define GOODIX_GTX8_TOUCH_DATA_ADDR_NORMANDY	0x4100
#define GOODIX_GTX8_TOUCH_DATA_ADDR_YELLOWSTONE	0x4180

#define I2C_MAX_TRANSFER_SIZE			256

enum goodix_gtx8_ic_type {
	IC_TYPE_NORMANDY,
	IC_TYPE_YELLOWSTONE,
};

struct goodix_gtx8_ic_data {
	size_t event_size;
	/*
	 * This is technically not the firmware version address
	 * referenced in the vendor driver, but rather the
	 * address of the product ID part. The meaning of the
	 * other parts is unknown and they are therefore omitted
	 * for now.
	 */
	int fw_version_addr;
	size_t header_size;
	enum goodix_gtx8_ic_type ic_type;
	char product_id[4];
	int touch_data_addr;
};

struct goodix_gtx8_header_normandy {
	u8 status;
	/* Only the lower 4 bits are actually used */
	u8 touch_num;
};
#define GOODIX_GTX8_HEADER_SIZE_NORMANDY \
	sizeof(struct goodix_gtx8_header_normandy)

struct goodix_gtx8_header_yellowstone {
	u8 status;
	/* Most likely unused */
	u8 __unknown1;
	/* Only the lower 4 bits are actually used */
	u8 touch_num;
	/* Most likely unused */
	u8 __unknown2[3];
	__le16 checksum;
} __packed __aligned(1);
#define GOODIX_GTX8_HEADER_SIZE_YELLOWSTONE \
	sizeof(struct goodix_gtx8_header_yellowstone)

struct goodix_gtx8_touch_normandy {
	u8 finger_id;
	__le16 x;
	__le16 y;
	u8 w;
	u8 __unknown[2];
} __packed __aligned(1);

struct goodix_gtx8_touch_yellowstone {
	/*
	 * Only the upper 4 bits are used, lower 4 bits are
	 * probably the sensor ID.
	 */
	u8 finger_id;
	u8 __unknown1;
	__be16 x;
	__be16 y;
	/*
	 * Vendor driver claims that this is a single __be16,
	 * but testing shows that it likely isn't.
	 */
	u8 __unknown2;
	u8 w;
} __packed __aligned(1);

union goodix_gtx8_touch {
	struct goodix_gtx8_touch_normandy normandy;
	struct goodix_gtx8_touch_yellowstone yellowstone;
};
#define GOODIX_GTX8_TOUCH_SIZE		sizeof(union goodix_gtx8_touch)

struct goodix_gtx8_event_normandy {
	struct goodix_gtx8_header_normandy hdr;
	/* The data below is u16 aligned */
	u8 data[GOODIX_GTX8_TOUCH_SIZE * GOODIX_GTX8_MAX_TOUCH +
		GOODIX_GTX8_CHECKSUM_SIZE];
};
#define GOODIX_GTX8_EVENT_SIZE_NORMANDY \
	sizeof(struct goodix_gtx8_event_normandy)

struct goodix_gtx8_event_yellowstone {
	struct goodix_gtx8_header_yellowstone hdr;
	/* The data below is u16 aligned */
	u8 data[GOODIX_GTX8_TOUCH_SIZE * GOODIX_GTX8_MAX_TOUCH +
		GOODIX_GTX8_CHECKSUM_SIZE];
};
#define GOODIX_GTX8_EVENT_SIZE_YELLOWSTONE \
	sizeof(struct goodix_gtx8_event_yellowstone)

struct goodix_gtx8_fw_version {
	/* 4 digits IC number */
	char product_id[4];
	/* Most likely unused */
	u8 __unknown[4];
	/* Four components version number */
	u8 fw_version[4];
};

struct goodix_gtx8_core {
	struct device *dev;
	struct regmap *regmap;
	struct regulator *avdd;
	struct regulator *vddio;
	struct gpio_desc *reset_gpio;
	struct touchscreen_properties props;
	struct goodix_gtx8_fw_version fw_version;
	struct input_dev *input_dev;
	int irq;
	const struct goodix_gtx8_ic_data *ic_data;
	u8 *event_buffer;
};

#endif
