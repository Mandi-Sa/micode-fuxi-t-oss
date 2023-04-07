/*
 * mi_ufsfbo_sysfs.c
 *
 * Created on: 2021-09-17
 *
 * Authors:
 *	lijiaming <lijiaming3@xiaomi.com>
 */

#include "mi_ufsfbo.h"
#include "mi_ufshcd.h"

void ufsfbo_remove_sysfs(struct ufsfbo_dev *fbo)
{
	int ret;

	ret = kobject_uevent(&fbo->kobj, KOBJ_REMOVE);
	FBO_INFO_MSG(fbo, "Kobject removed (%d)", ret);
	kobject_del(&fbo->kobj);
}

static ssize_t ufsfbo_sysfs_show_fbo_support(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_support (%d)", fbo->fbo_support);

	return sysfs_emit(buf, "%d\n", fbo->fbo_support);
}

static ssize_t ufsfbo_sysfs_show_fbo_version(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_version (%04x)", fbo->fbo_version);

	return sysfs_emit(buf, "%04x\n", fbo->fbo_version);
}

static ssize_t ufsfbo_sysfs_show_fbo_rec_lrs(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_rec_lrs (%d)", fbo->fbo_rec_lrs);

	return sysfs_emit(buf, "%d\n", fbo->fbo_rec_lrs);
}

static ssize_t ufsfbo_sysfs_show_fbo_max_lrs(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_max_lrs (%d)", fbo->fbo_max_lrs);

	return sysfs_emit(buf, "%d\n", fbo->fbo_max_lrs);
}

static ssize_t ufsfbo_sysfs_show_fbo_min_lrs(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_min_lrs (%d)", fbo->fbo_min_lrs);

	return sysfs_emit(buf, "%d\n", fbo->fbo_min_lrs);
}

static ssize_t ufsfbo_sysfs_show_fbo_max_lrc(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_max_lrc (%d)", fbo->fbo_max_lrc);

	return sysfs_emit(buf, "%d\n", fbo->fbo_max_lrc);
}

static ssize_t ufsfbo_sysfs_show_fbo_lra(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_lra (%d)", fbo->fbo_lra);

	return sysfs_emit(buf, "%d\n", fbo->fbo_lra);
}

static ssize_t ufsfbo_sysfs_show_fbo_prog_state(struct ufsfbo_dev *fbo, char *buf)
{
	int ret, fbo_prog_state;

	if (fbo->is_ufs31)
		return -ENOTSUPP;

	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	ret = ufsfbo_get_fbo_prog_state(fbo, &fbo_prog_state);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);
	if (ret) {
		FBO_ERR_MSG("Get fbo_prog_state failed");
		return -EINVAL;
	}
	return sysfs_emit(buf, "%d\n", fbo_prog_state);
}

static ssize_t ufsfbo_sysfs_show_fbo_get_lr_frag_level(struct ufsfbo_dev *fbo,
						  char *buf)
{
	int i, ret, count = 0;
	int vaild_body_size = 0;
	char *fbo_read_buffer;

	fbo_read_buffer = kzalloc(FBO_LBA_RANGE_LENGTH, GFP_KERNEL);
	if (!fbo_read_buffer) {
		ret = -ENOMEM;
		return ret;
	}
	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	ret = ufsfbo_read_frag_level(fbo, fbo_read_buffer);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);
	if (ret) {
		FBO_ERR_MSG("Get lba range level failed");
		goto out;
	}

	vaild_body_size = FBO_BODY_HEADER_SIZE + (fbo->fbo_lba_count * FBO_BODY_ENTRY_SIZE);
	if (fbo->is_ufs31) {
		for(i = 0; i < vaild_body_size; i++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%02x ", fbo_read_buffer[i]);
			if(!((i + 1 ) % 8))
				count += snprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	} else {
		for(i = 0; i < vaild_body_size; i++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%02x ", fbo_read_buffer[i + FBO_HEADER_SIZE]);
			if(!((i + 1 ) % 8))
				count += snprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	}

out:
	kfree(fbo_read_buffer);
	return count;
}

static ssize_t ufsfbo_sysfs_show_fbo_wholefile_enable(struct ufsfbo_dev *fbo,
						  char *buf)
{
	return sysfs_emit(buf, "whole file flag: %d\n", fbo->fbo_wholefile);
}

static ssize_t ufsfbo_sysfs_store_fbo_wholefile_enable(struct ufsfbo_dev *fbo,
						   const char *buf,
						   size_t count)
{
	bool val;

	if (kstrtobool(buf, &val)) {
		FBO_ERR_MSG("Convert bool type fail from char * type");
		return -EINVAL;
	}

	fbo->fbo_wholefile = val;

	return count;
}

static ssize_t ufsfbo_sysfs_show_fbo_err_cnt(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_err_cnt:%d", fbo->fbo_err_cnt);

	return snprintf(buf, PAGE_SIZE, "%d\n", fbo->fbo_err_cnt);
}

static ssize_t ufsfbo_sysfs_store_fbo_err_cnt(struct ufsfbo_dev *fbo, const char *buf,
					size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	fbo->fbo_err_cnt += val;

	FBO_INFO_MSG(fbo, "fbo_err_cnt:%d", fbo->fbo_err_cnt);

	return count;
}

static ssize_t ufsfbo_sysfs_show_fbo_retry_cnt(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "fbo_retry_cnt:%d", fbo->fbo_retry_cnt);

	return snprintf(buf, PAGE_SIZE, "%d\n", fbo->fbo_retry_cnt);
}

static ssize_t ufsfbo_sysfs_store_fbo_retry_cnt(struct ufsfbo_dev *fbo, const char *buf,
					size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	fbo->fbo_retry_cnt += val;

	FBO_INFO_MSG(fbo, "fbo_retry_cnt:%d", fbo->fbo_retry_cnt);

	return count;
}

static ssize_t ufsfbo_sysfs_show_fbo_exe_threshold(struct ufsfbo_dev *fbo,
						  char *buf)
{
	int frag_exe_level, ret;

	ret = ufsfbo_get_exe_level(fbo, &frag_exe_level);
	if (ret) {
		FBO_ERR_MSG("Get execute threshold failed");
		return -EINVAL;
	}
	return sysfs_emit(buf, "%d\n", frag_exe_level);
}

static ssize_t ufsfbo_sysfs_store_fbo_exe_threshold(struct ufsfbo_dev *fbo,
						   const char *buf,
						   size_t count)
{
	unsigned long val;
	int ret = 0, fbo_prog_state, defrag_ops;


	if (kstrtoul(buf, 0, &val)) {
		return -EINVAL;
	}

	if (val < 0 || val > 10) {
		FBO_ERR_MSG("fbo_exe_threshold set error, illegal value");
		return -EINVAL;
	}

	if (fbo->is_ufs31){
		ret = ufsfbs_get_ops(fbo, &defrag_ops);
		if (ret) {
			FBO_ERR_MSG("Get defrag opstatus failed");
			return -EINVAL;
		}
		if (defrag_ops ==UFSFBS_OPS_HOST_NA  || defrag_ops == UFSFBS_OPS_DEVICE_NA) {
			ret = ufsfbo_set_exe_level(fbo, (int *)&val);
			if (ret) {
				FBO_ERR_MSG("Get execute level failed");
				return -EINVAL;
			}
			FBO_INFO_MSG(fbo, "fbs_set_exe_level:%d", val);
		} else {
			FBO_ERR_MSG("fbs_exe_level set error, illegal defrag ops value");
			return -EINVAL;
		}
	} else {
		ret = ufsfbo_get_fbo_prog_state(fbo, &fbo_prog_state);
		if (ret) {
			FBO_ERR_MSG("Get fbo prog state failed");
			return -EINVAL;
		}

		if (fbo_prog_state == FBO_PROG_IDLE || fbo_prog_state == FBO_PROG_ANALYSIS_COMPLETE ||
			fbo_prog_state == FBO_PROG_OPTIMIZATION_COMPLETE) {
			ret = ufsfbo_set_exe_level(fbo, (int *)&val);
			if (ret) {
				FBO_ERR_MSG("Get execute level failed");
				return -EINVAL;
			}
			FBO_INFO_MSG(fbo, "fbo_set_exe_threshold %d", val);
		} else {
			FBO_ERR_MSG("fbo_exe_threshold set error, illegal fbo prog state");
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t ufsfbo_sysfs_show_debug(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "Debug:%d", fbo->fbo_debug);

	return sysfs_emit(buf, "%d\n", fbo->fbo_debug);
}

static ssize_t ufsfbo_sysfs_store_debug(struct ufsfbo_dev *fbo, const char *buf,
					size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	fbo->fbo_debug = val ? true : false;

	FBO_INFO_MSG(fbo, "Debug:%d", fbo->fbo_debug);

	return count;
}

static ssize_t ufsfbo_sysfs_show_block_suspend(struct ufsfbo_dev *fbo,
					       char *buf)
{
	FBO_INFO_MSG(fbo, "Block suspend:%d", fbo->block_suspend);

	return sysfs_emit(buf, "%d\n", fbo->block_suspend);
}

static ssize_t ufsfbo_sysfs_store_block_suspend(struct ufsfbo_dev *fbo,
						const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	FBO_INFO_MSG(fbo, "fbo_block_suspend %lu", val);

	if (val == fbo->block_suspend)
		return count;

	if (val)
		ufsfbo_block_enter_suspend(fbo);
	else
		ufsfbo_allow_enter_suspend(fbo);

	fbo->block_suspend = val ? true : false;

	return count;
}

static ssize_t ufsfbo_sysfs_show_auto_hibern8_enable(struct ufsfbo_dev *fbo,
						     char *buf)
{
	FBO_INFO_MSG(fbo, "HCI auto hibern8 %d", fbo->is_auto_enabled);

	return sysfs_emit(buf, "%d\n", fbo->is_auto_enabled);
}

static ssize_t ufsfbo_sysfs_store_auto_hibern8_enable(struct ufsfbo_dev *fbo,
						      const char *buf,
						      size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	ufsfbo_auto_hibern8_enable(fbo, val);

	return count;
}

static ssize_t ufsfbo_sysfs_store_fbo_operation_control(struct ufsfbo_dev *fbo,
						      const char *buf,
						      size_t count)
{
	int ret = 0;
	unsigned long val;

	if (kstrtoul(buf, 0, &val)) {
		return -EINVAL;
	}

	if (fbo->is_ufs31)
		return -ENOTSUPP;

	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	FBO_INFO_MSG(fbo, "Defrag_enable start power mode: %d", fbo->hba->curr_dev_pwr_mode);
	/*
		bFBOControl  val
		0x1:Enable Frag level check
		0x2:Enable Defrag
	*/
	FBO_INFO_MSG(fbo, "fbo control val = 0x%x", val);
	ret = ufsfbo_operation_control(fbo, (int *)&val);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);

	if (ret) {
		FBO_ERR_MSG("Enable frag level check failed");
		return -EINVAL;
	}

	return count;
}

static int ufsfbo_check_lr_list_buf(struct ufsfbo_dev *fbo, const char *buf)
{
	char *arg;
	int len = 0;

	if(!buf) {
		FBO_ERR_MSG("Invalid fbo write buf input, please try again");
		return -EINVAL;
	}

	arg = strstr(buf, ",");
	if(arg == NULL || buf[strlen(buf) - 1] == ',') {
		FBO_ERR_MSG("Invalid lba range, please input lba range separated by ','");
		return -EINVAL;
	}

	while (arg != NULL) {
		len++;
		arg +=1;
		arg = strstr(arg, ",");
	}
	if (len%2) {
		len++;
		FBO_INFO_MSG(fbo, "Valid lba range count");
	} else {
		FBO_ERR_MSG("Invalid lba range count, please input again");
		return -EINVAL;
	}
	fbo->fbo_lba_count = len/2;
	return 0;
}

static ssize_t ufsfbo_sysfs_store_fbo_send_lr_list(struct ufsfbo_dev *fbo,
						      const char *buf,
						      size_t count)
{
	int ret = 0, fbo_prog_state = 0;
	int level_ops = 0, defrag_ops = 0;

	ret = ufsfbo_check_lr_list_buf(fbo, buf);
	if (ret) {
		FBO_ERR_MSG("L-range list check fail");
		return -EINVAL;
	}

	if (fbo->is_ufs31) {
		ret = ufsfbs_get_level_check_ops(fbo, &level_ops);
		if (ret) {
			FBO_ERR_MSG("Get level_ops fail");
			return -EINVAL;
		}

		ret = ufsfbs_get_ops(fbo, &defrag_ops);
		if (ret) {
			FBO_ERR_MSG("Get defrag_ops fail");
			return -EINVAL;
		}

		if(level_ops == UFSFBS_OPS_SUCCESS){
			FBO_INFO_MSG(fbo, "level_ops:%d, abnormal, clear flag again", level_ops);
			ufsfbs_frag_level_check_enable(fbo, 0);
		}

		if(level_ops == UFSFBS_OPS_HOST_NA && defrag_ops == UFSFBS_OPS_HOST_NA) {
			ufsfbo_block_enter_suspend(fbo);
			ufsfbo_auto_hibern8_enable(fbo, 0);

			ret = ufsfbo_lba_list_write(fbo, buf);

			ufsfbo_auto_hibern8_enable(fbo, 1);
			ufsfbo_allow_enter_suspend(fbo);
			if (ret) {
				FBO_ERR_MSG("Send lba range failed");
				return -EINVAL;
			}
		} else {
			FBO_ERR_MSG("Invalid defrag or level check ops");
		}
	} else {
		ret = ufsfbo_get_fbo_prog_state(fbo, &fbo_prog_state);
		if (ret) {
			FBO_ERR_MSG("Invalid fbo_prog_state");
			return -EINVAL;
		}

		if(fbo_prog_state == FBO_PROG_IDLE) {
			ufsfbo_block_enter_suspend(fbo);
			ufsfbo_auto_hibern8_enable(fbo, 0);

			ret = ufsfbo_lba_list_write(fbo, buf);

			ufsfbo_auto_hibern8_enable(fbo, 1);
			ufsfbo_allow_enter_suspend(fbo);
			if (ret) {
				FBO_ERR_MSG("Send lba range failed");
				return -EINVAL;
			}
		} else {
			FBO_ERR_MSG("Invalid defrag or level check ops");
		}
	}

	return count;
}

static ssize_t ufsfbo_sysfs_show_fbs_flc_ops(struct ufsfbo_dev *fbo, char *buf)
{
	int ret, level_ops;

	if (!fbo->is_ufs31)
		return -ENOTSUPP;

	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	ret = ufsfbs_get_level_check_ops(fbo, &level_ops);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);

	if (ret) {
		FBO_ERR_MSG("Get level check opstatus failed");
		return -EINVAL;
	}
	return sysfs_emit(buf, "%d\n", level_ops);
}

static ssize_t ufsfbo_sysfs_show_fbs_defrag_ops(struct ufsfbo_dev *fbo, char *buf)
{
	int ret, defrag_ops;

	if (!fbo->is_ufs31)
		return -ENOTSUPP;

	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	ret = ufsfbs_get_ops(fbo, &defrag_ops);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);

	if (ret) {
		FBO_ERR_MSG("Get defrag opstatus failed");
		return -EINVAL;
	}
	return sysfs_emit(buf, "%d\n", defrag_ops);
}

static ssize_t ufsfbo_sysfs_store_fbs_flc_enable(struct ufsfbo_dev *fbo,
						      const char *buf,
						      size_t count)
{
	bool val;
	int ret;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (!fbo->is_ufs31)
		return -ENOTSUPP;

	ufsfbo_block_enter_suspend(fbo);
	ufsfbo_auto_hibern8_enable(fbo, 0);

	ret = ufsfbs_frag_level_check_enable(fbo, val);

	ufsfbo_auto_hibern8_enable(fbo, 1);
	ufsfbo_allow_enter_suspend(fbo);

	if (ret) {
		FBO_ERR_MSG("Enable frag level check failed");
		return -EINVAL;
	}

	return count;
}

static ssize_t ufsfbo_sysfs_store_fbs_defrag_enable(struct ufsfbo_dev *fbo,
						      const char *buf,
						      size_t count)
{
	bool val;
	int ret = 0, defrag_ops = 0;

	if (kstrtobool(buf, &val)) {
		return -EINVAL;
	}
	if (!fbo->is_ufs31)
		return -ENOTSUPP;

	ret = ufsfbs_get_ops(fbo, &defrag_ops);
	if (ret) {
		FBO_ERR_MSG("Get defrag ops failed");
		return -EINVAL;
	}
	if ((fbo->vendor == UFS_HYNIX) || (defrag_ops == UFSFBS_OPS_PROGRESSING && !val) || val) {
		ufsfbo_block_enter_suspend(fbo);
		ufsfbo_auto_hibern8_enable(fbo, 0);

		ret = ufsfbs_defrag_enable(fbo, val);

		ufsfbo_auto_hibern8_enable(fbo, 1);
		ufsfbo_allow_enter_suspend(fbo);

		if (ret) {
			FBO_ERR_MSG("Enable level check failed");
			return -EINVAL;
		}
	} else {
		FBO_ERR_MSG("Invalid degrag_ops(%d), or enable value(%d) \n", defrag_ops, val);
	}
	return  count;
}

static ssize_t ufsfbo_sysfs_show_is_ufs31(struct ufsfbo_dev *fbo, char *buf)
{
	FBO_INFO_MSG(fbo, "is_ufs31:%d", fbo->is_ufs31);

	return sysfs_emit(buf, "%d\n", fbo->is_ufs31);
}

/* SYSFS DEFINE */
#define define_sysfs_ro(_name) __ATTR(_name, 0444,			\
				      ufsfbo_sysfs_show_##_name, NULL)
#define define_sysfs_wo(_name) __ATTR(_name, 0200,			\
				       NULL, ufsfbo_sysfs_store_##_name)
#define define_sysfs_rw(_name) __ATTR(_name, 0644,			\
				      ufsfbo_sysfs_show_##_name,	\
				      ufsfbo_sysfs_store_##_name)

static struct ufsfbo_sysfs_entry ufsfbo_sysfs_entries[] = {
	define_sysfs_ro(fbo_rec_lrs),
	define_sysfs_ro(fbo_max_lrs),
	define_sysfs_ro(fbo_min_lrs),
	define_sysfs_ro(fbo_max_lrc),
	define_sysfs_ro(fbo_lra),
	define_sysfs_ro(fbo_prog_state),
	define_sysfs_ro(fbo_get_lr_frag_level),
	define_sysfs_ro(fbo_support),
	define_sysfs_ro(fbo_version),
	/*for UFS 3.1*/
	define_sysfs_ro(fbs_flc_ops),
	define_sysfs_ro(fbs_defrag_ops),
	define_sysfs_wo(fbs_flc_enable),
	define_sysfs_wo(fbs_defrag_enable),
	define_sysfs_ro(is_ufs31),

	define_sysfs_wo(fbo_operation_control),
	define_sysfs_wo(fbo_send_lr_list),

	define_sysfs_rw(fbo_exe_threshold),
	define_sysfs_rw(fbo_wholefile_enable),
	define_sysfs_rw(fbo_err_cnt),
	define_sysfs_rw(fbo_retry_cnt),
	/* debug */
	define_sysfs_rw(debug),
	/* Attribute (RAW) */
	define_sysfs_rw(block_suspend),
	define_sysfs_rw(auto_hibern8_enable),
	__ATTR_NULL
};

static ssize_t ufsfbo_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufsfbo_sysfs_entry *entry;
	struct ufsfbo_dev *fbo;
	ssize_t error;

	entry = container_of(attr, struct ufsfbo_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	fbo = container_of(kobj, struct ufsfbo_dev, kobj);
	if (ufsfbo_is_not_present(fbo))
		return -ENODEV;

	mutex_lock(&fbo->sysfs_lock);
	error = entry->show(fbo, page);
	mutex_unlock(&fbo->sysfs_lock);

	return error;
}

static ssize_t ufsfbo_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t length)
{
	struct ufsfbo_sysfs_entry *entry;
	struct ufsfbo_dev *fbo;
	ssize_t error;

	entry = container_of(attr, struct ufsfbo_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	fbo = container_of(kobj, struct ufsfbo_dev, kobj);
	if (ufsfbo_is_not_present(fbo))
		return -ENODEV;

	mutex_lock(&fbo->sysfs_lock);
	error = entry->store(fbo, page, length);
	mutex_unlock(&fbo->sysfs_lock);

	return error;
}

static const struct sysfs_ops ufsfbo_sysfs_ops = {
	.show = ufsfbo_attr_show,
	.store = ufsfbo_attr_store,
};

static struct kobj_type ufsfbo_ktype = {
	.sysfs_ops = &ufsfbo_sysfs_ops,
	.release = NULL,
};

 int ufsfbo_create_sysfs(struct ufsfbo_dev *fbo)
{
	struct device *dev = fbo->hba->dev;
	struct ufsfbo_sysfs_entry *entry;
	int err;

	fbo->sysfs_entries = ufsfbo_sysfs_entries;

	kobject_init(&fbo->kobj, &ufsfbo_ktype);
	mutex_init(&fbo->sysfs_lock);

	FBO_INFO_MSG(fbo, "Creates sysfs %p dev->kobj %p",
		 &fbo->kobj, &dev->kobj);

	err = kobject_add(&fbo->kobj, kobject_get(&dev->kobj), "ufsfbo");
	if (!err) {
		for (entry = fbo->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			FBO_INFO_MSG(fbo, "Sysfs attr creates: %s",
				 entry->attr.name);
			err = sysfs_create_file(&fbo->kobj, &entry->attr);
			if (err) {
				FBO_ERR_MSG("Create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&fbo->kobj, KOBJ_ADD);
	} else {
		FBO_ERR_MSG("Kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&fbo->kobj, KOBJ_REMOVE);
	FBO_INFO_MSG(fbo, "Kobject removed (%d)", err);
	kobject_del(&fbo->kobj);
	return -EINVAL;
}
