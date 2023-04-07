
/*
 * Copyright (C) 2021 Xiaomi Ltd.
 *
 * This is the interface used to set UFS parameters
 *
 * Author:
 *    lijiaming3 <lijiaming3@xiaomi.com>
 */
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/dev_printk.h>
#include <linux/kernel.h>
enum EBUFFHA_STATUS {
	EBUFFHA_FALSE = 0,
	EBUFFHA_TRUE = 1,
	EBUFFHA_NA = 2,
};
enum EBUFFHA_VALUE_FLAG {
	EBUFFHA_VALUE_BOOL = 0,
	EBUFFHA_VALUE_DIGIT = 1,
	EBUFFHA_VALUE_CHAR = 2,
	EBUFFHA_VALUE_NA = 3,
};
struct ebuffha {
	char name[24];
	char pname[24];
	enum EBUFFHA_VALUE_FLAG value_flag;
	bool enable_flag;
	u32 value;
};
struct ebuffha ebuffha_data[] = {
	{"VCC", "vcc", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"RATE", "rate", EBUFFHA_VALUE_CHAR, false, 0 },
	{"GEAR", "gear", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"LANES", "lanes_per_dir", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"TACTIVE_DEVICE", "tActive_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"TACTIVE_HOST", "tActive_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"THIBERN8", "tHibernate8", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"DEVICEQUIRKS", "device_quirks", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"DEVQUIRKSMASK", "dev_quirks_mask", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HOSTQUIRKS", "host_quirks", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HOSTQUIRKSMASK", "host_quirks_mask", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"CAPS", "caps", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"CAPSMASK", "caps_mask", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"DISABLELPM", "disable_lpm", EBUFFHA_VALUE_BOOL, false, EBUFFHA_NA },
	{"DISABLEAUTOH8", "disable_autoh8", EBUFFHA_VALUE_BOOL, false, EBUFFHA_NA },
	{"VCCALWAYSON", "vcc_always_on", EBUFFHA_VALUE_BOOL, false, EBUFFHA_NA },
	{"VCCQALWAYSON", "vccq_always_on", EBUFFHA_VALUE_BOOL, false, EBUFFHA_NA },
	{"HSG1SYNCLENGTHHOST", "hsg1_synclength_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG2SYNCLENGTHHOST", "hsg2_synclength_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG3SYNCLENGTHHOST", "hsg3_synclength_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG4SYNCLENGTHHOST", "hsg4_synclength_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG5SYNCLENGTHHOST", "hsg5_synclength_host", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG1SYNCLENGTHDEVICE", "hsg1_synclength_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG2SYNCLENGTHDEVICE", "hsg2_synclength_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG3SYNCLENGTHDEVICE", "hsg3_synclength_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG4SYNCLENGTHDEVICE", "hsg4_synclength_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"HSG5SYNCLENGTHDEVICE", "hsg5_synclength_device", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"REFCLKALWAYSON", "ref_clk_always_on", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"DE-EMPHASIS", "de-emphasis", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"TX-LANE", "tx-lane", EBUFFHA_VALUE_DIGIT, false, 0 },
	{"RX-LANE", "rx-lane", EBUFFHA_VALUE_DIGIT, false, 0 },
};
bool ebuff_value_u32(char *pname, u32 *value)
{
	char *ptr = pname;
	int i = 0;
	bool enable_flag = false;
	for (i = 0; i < sizeof(ebuffha_data) / sizeof(struct ebuffha); i++) {
		if (ebuffha_data[i].enable_flag && !strncmp(ebuffha_data[i].pname, ptr, sizeof(ebuffha_data[i].pname))) {
			*value = ebuffha_data[i].value;
			enable_flag = ebuffha_data[i].enable_flag;
			break;
		}
	}
	return enable_flag;
}
static bool str2u32(char *buff, u32 *value)
{
	return !kstrtou32((const char *)buff, 0, value);
}
bool of_obtain_ebuffha_info(void)
{
	struct device_node *ebuff_node;
	char *buff = NULL;
	int i = 0;
	int ret = 0;
	u32 value = 0;
	ebuff_node = of_find_node_by_path("/memory/ebuff");
	if (!ebuff_node)
		return false;
	for (i = 0; i < sizeof(ebuffha_data) / sizeof(struct ebuffha); i++) {
		ret = of_property_read_string(ebuff_node, ebuffha_data[i].name, (const char **)&buff);
		if (ret || !buff)
			continue;
		if (ebuffha_data[i].value_flag == EBUFFHA_VALUE_BOOL) {
			if (!strncmp(buff, "true", 4) || !strncmp(buff, "TRUE", 4)) {
				ebuffha_data[i].value = EBUFFHA_TRUE;
				ebuffha_data[i].enable_flag = true;
			} else if (!strncmp(buff, "false", 5) || !strncmp(buff, "FALSE", 5)) {
				ebuffha_data[i].value = EBUFFHA_FALSE;
				ebuffha_data[i].enable_flag = true;
			}
		} else if (ebuffha_data[i].value_flag == EBUFFHA_VALUE_CHAR) {
			if (!strncmp(buff, "A", 1) || !strncmp(buff, "a", 1)) {
				ebuffha_data[i].value = 1;
				ebuffha_data[i].enable_flag = true;
			} else if (!strncmp(buff, "B", 1) || !strncmp(buff, "b", 1)) {
				ebuffha_data[i].value = 2;
				ebuffha_data[i].enable_flag = true;
			}
		} else if (ebuffha_data[i].value_flag == EBUFFHA_VALUE_DIGIT) {
			if (str2u32(buff, &value)) {
				ebuffha_data[i].value = value;
				ebuffha_data[i].enable_flag = true;
			}
		}
		pr_err("ebuff ufs host adapter %s %d\n", ebuffha_data[i].name, ebuffha_data[i].value);
	}
	of_node_put(ebuff_node);
	return true;
}
