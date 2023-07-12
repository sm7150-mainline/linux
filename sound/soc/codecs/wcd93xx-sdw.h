// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021, Linaro Limited

#define WCD_SDW_CH(id, pn, cmask)	\
	[id] = {			\
		.port_num = pn,		\
		.ch_mask = cmask,	\
	}

#if IS_ENABLED(CONFIG_SND_SOC_WCD938X_SDW)
struct device *wcd93xx_sdw_device_get(struct device_node *np);
int wcd93xx_swr_get_current_bank(struct sdw_slave *sdev);
#else
struct device *wcd93xx_sdw_device_get(struct device_node *np)
{
	return -EOPNOTSUPP;
}

int wcd93xx_swr_get_current_bank(struct sdw_slave *sdev)
{
	return -EOPNOTSUPP;
}
#endif
