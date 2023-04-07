/*
 * mi_ufsfbo.c
 *
 * Created on: 2021-09-17
 *
 * Authors:
 *	lijiaming <lijiaming3@xiaomi.com>
 */
#include <linux/pm_runtime.h>
#include "mi_ufsfbo.h"
#include "mi_ufsfbo_sysfs.h"
#include "mi_ufshcd.h"
#include "mi_ufs.h"
#include "../ufs/ufs-qcom.h"

static int ufsfbo_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			u8 selector, u8 *desc_buf, u32 size)
{
	int ret = 0;
	int retries;
	ufshcd_rpm_get_sync(hba);

	for (retries = 3; retries > 0; retries--) {
		ret = __ufshcd_query_descriptor(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index,
					    selector,
					    desc_buf, &size);
		if (!ret || ret == -EINVAL)
			break;
        }

	if (ret)
		FBO_ERR_MSG("Read desc [0x%.2X] failed. (%d)", desc_id, ret);

	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);

	return ret;
}

static int ufsfbo_query_attr(struct ufs_hba *hba, enum query_opcode opcode, u8 idn,
			u8 idx, u8 selector, u32 *attr_val)
{
	int ret = 0;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_attr_retry(hba, opcode, idn, idx,
				      selector, attr_val);
	if (ret)
		FBO_ERR_MSG("Query attr [0x%.2X] failed. opcode(%d) (%d)", idn, opcode, ret);

	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);

	return ret;
}

static int ufsfbo_query_flag(struct ufs_hba *hba, enum query_opcode opcode, u8 idn,
			u8 idx, u8 selector, bool *flag_res)
{
	int ret = 0;
	int retries;
	ufshcd_rpm_get_sync(hba);

	for (retries = 0; retries < 3; retries++) {
		ret = ufshcd_query_flag_sel(hba, opcode, idn, idx, selector, flag_res);
		if (ret)
			FBO_ERR_MSG("Query flag [0x%.2X] failed. opcode(%d) (%d)", idn, opcode, ret);
		else
			break;
	}

	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);

	return ret;
}

int ufsfbo_is_not_present(struct ufsfbo_dev *fbo)
{
	enum UFSFBO_STATE cur_state = ufsfbo_get_state(fbo->hba);

	if (cur_state != FBO_PRESENT) {
		FBO_INFO_MSG(fbo, "fbo_state != fbo_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}

inline int ufsfbo_get_state(struct ufs_hba *hba)
{
	return atomic_read(&hba->fbo.fbo_state);
}

inline void ufsfbo_set_state(struct ufs_hba *hba, int state)
{
	atomic_set(&hba->fbo.fbo_state, state);
}

int ufsfbo_operation_control(struct ufsfbo_dev *fbo, int *val)
{
	int ret = 0;
	struct ufs_hba *hba = fbo->hba;

	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBO_CONTROL, 0, 0, val);
	if (ret)
		FBO_ERR_MSG("Query fbo control attr failed. ret(%d)", ret);

	return ret;
}

void ufsfbo_auto_hibern8_enable(struct ufsfbo_dev *fbo,
				       unsigned int val)
{
	struct ufs_hba *hba = fbo->hba;
	unsigned long flags;
	u32 reg;

	val = !!val;

	/* Update auto hibern8 timer value if supported */
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;
	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);
	down_write(&hba->clk_scaling_lock);
	ufshcd_scsi_block_requests(hba);
	/* wait for all the outstanding requests to finish */
	ufshcd_wait_for_doorbell_clr(hba, U64_MAX);
	spin_lock_irqsave(hba->host->host_lock, flags);

	reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
	FBO_INFO_MSG(fbo, "ahit-reg:0x%X", reg);

	if (val ^ (reg != 0)) {
		if (val) {
			hba->ahit = fbo->ahit;
		} else {
			/*
			 * Store current ahit value.
			 * We don't know who set the ahit value to different
			 * from the initial value
			 */
			fbo->ahit = reg;
			hba->ahit = 0;
		}
		ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
		/* Make sure the timer gets applied before further operations */
		mb();

		FBO_INFO_MSG(fbo, "Before is_auto_enabled:%d", fbo->is_auto_enabled);
		fbo->is_auto_enabled = val;

		reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
		FBO_INFO_MSG(fbo, "After is_auto_enabled:%d,ahit-reg:0x%X",
			 fbo->is_auto_enabled, reg);
	} else {
		FBO_INFO_MSG(fbo, "is_auto_enabled:%d. so it does not changed",
			 fbo->is_auto_enabled);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_scsi_unblock_requests(hba);
	up_write(&hba->clk_scaling_lock);
	ufshcd_release(hba);
	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);
}

void ufsfbo_block_enter_suspend(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->hba;
	unsigned long flags;

	if (unlikely(fbo->block_suspend))
		return;

	fbo->block_suspend = true;

	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);

	spin_lock_irqsave(hba->host->host_lock, flags);
	FBO_INFO_MSG(fbo,
		  "power.usage_count:%d,clk_gating.active_reqs:%d",
		  atomic_read(&hba->dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

void ufsfbo_allow_enter_suspend(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->hba;
	unsigned long flags;

	if (unlikely(!fbo->block_suspend))
		return;

	fbo->block_suspend = false;

	ufshcd_release(hba);
	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);

	spin_lock_irqsave(hba->host->host_lock, flags);
	FBO_INFO_MSG(fbo,
		  "power.usage_count:%d,clk_gating.active_reqs:%d",
		  atomic_read(&hba->dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

int ufsfbo_get_exe_level(struct ufsfbo_dev *fbo, int *frag_exe_level)
{
	int ret = 0;
	struct ufs_hba *hba = fbo->hba;

	if (fbo->is_ufs31)
		ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_LEVEL_EXE, 0, 0, frag_exe_level);
	else
		ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBO_LEVEL_EXE, 0, 0, frag_exe_level);

	if (ret)
		FBO_ERR_MSG("Get_exe_level failed, ret(%d)", ret);
	return ret;
}

int ufsfbo_set_exe_level(struct ufsfbo_dev *fbo, int *frag_exe_level)
{
	int ret = 0;
	struct ufs_hba *hba = fbo->hba;

	if (fbo->is_ufs31)
		ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_LEVEL_EXE, 0, 0, frag_exe_level);
	else
		ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBO_LEVEL_EXE, 0, 0, frag_exe_level);

	if (ret)
		FBO_ERR_MSG("Set_exe_level failed, ret(%d)", ret);
	return ret;
}

int ufsfbs_get_level_check_ops(struct ufsfbo_dev *fbo, int *level_ops)
{
	struct ufs_hba *hba = fbo->hba;
	int ret = 0, attr = -1;

	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_FLC_OPS, 0, 0, &attr);
	if (ret)
		return ret;
	if (attr == 0x0) {
		*level_ops = UFSFBS_OPS_HOST_NA;
	} else if (attr == 0x1) {
		*level_ops = UFSFBS_OPS_DEVICE_NA;
	} else if (attr == 0x2) {
		*level_ops = UFSFBS_OPS_PROGRESSING;
	} else if (attr == 0x3) {
		*level_ops = UFSFBS_OPS_SUCCESS;
	} else if (attr == 0x4) {
		*level_ops = UFSFBS_OPS_PRE_MATURELY;
	} else if (attr == 0x5) {
		*level_ops = UFSFBS_OPS_HOST_LBA_NA;
	} else if (attr == 0xff) {
		*level_ops = UFSFBS_OPS_INTERNAL_ERR;
	} else {
		FBO_INFO_MSG(fbo, "unknown level ops %d\n", attr);
		ret = -1;
		return ret;
	}
	return 0;
}

int ufsfbs_get_ops(struct ufsfbo_dev *fbo, int *defrag_ops)
{
	struct ufs_hba *hba = fbo->hba;
	int ret = 0, attr = -1;

	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_DEFRAG_OPS, 0, 0, &attr);
	if (ret)
		return ret;
	if (attr == 0x0) {
		*defrag_ops = UFSFBS_OPS_HOST_NA;
	} else if (attr == 0x1) {
		*defrag_ops = UFSFBS_OPS_DEVICE_NA;
	} else if (attr == 0x2) {
		*defrag_ops = UFSFBS_OPS_PROGRESSING;
	} else if (attr == 0x3) {
		*defrag_ops = UFSFBS_OPS_SUCCESS;
	} else if (attr == 0x4) {
		*defrag_ops = UFSFBS_OPS_PRE_MATURELY;
	} else if (attr == 0x5) {
		*defrag_ops = UFSFBS_OPS_HOST_LBA_NA;
	} else if (attr == 0xff) {
		*defrag_ops = UFSFBS_OPS_INTERNAL_ERR;
	} else {
		FBO_INFO_MSG(fbo, "unknown defrag ops %d\n", attr);
		ret = -1;
		return ret;
	}
	return 0;
}

int ufsfbs_frag_level_check_enable(struct ufsfbo_dev *fbo, bool level_check_enable)
{
	struct ufs_hba *hba = fbo->hba;
	u8 idn = QUERY_ATTR_IDN_FBS_FLC_ENABLE;

	if (fbo->vendor == UFS_MICRON)
		idn = QUERY_ATTR_IDN_MICRON_FBS_FLC_ENABLE;

	if(level_check_enable)
		return ufsfbo_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum flag_idn)idn, 0, 0, &level_check_enable);
	else
		return ufsfbo_query_flag(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, (enum flag_idn)idn, 0, 0, NULL);
}

int ufsfbs_defrag_enable(struct ufsfbo_dev *fbo, bool defrag_enable)
{
	struct ufs_hba *hba = fbo->hba;
	u8 idn = QUERY_ATTR_IDN_FBS_DEFRAG_ENABLE;

	if (fbo->vendor == UFS_MICRON)
		idn = QUERY_ATTR_IDN_MICRON_FBS_DEFRAG_ENABLE;

	if(defrag_enable)
		return ufsfbo_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum flag_idn)idn, 0, 0, &defrag_enable);
	else
		return ufsfbo_query_flag(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, (enum flag_idn)idn, 0, 0, NULL);
}

int ufsfbo_get_fbo_prog_state(struct ufsfbo_dev *fbo, int *prog_state)
{
	struct ufs_hba *hba = fbo->hba;
	int ret = 0, attr = -1;

	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBO_PROG_STATE, 0, 0, &attr);
	if (ret){
		FBO_ERR_MSG("Get fbo_prog_state failed, ret(%d)", ret);
		return ret;
	}
	switch(attr){
	case 0x0:
		*prog_state = FBO_PROG_IDLE;
		break;
	case 0x1:
		*prog_state = FBO_PROG_ON_GOING;
		break;
	case 0x2:
		*prog_state = FBO_PROG_ANALYSIS_COMPLETE;
		break;
	case 0x3:
		*prog_state = FBO_PROG_OPTIMIZATION_COMPLETE;
		break;
	case 0xff:
		*prog_state = FBO_PROG_INTERNAL_ERR;
		break;
	default:
		FBO_INFO_MSG(fbo, "fbo unknown prog state attr(%d)", attr);
		ret = -1;
		break;
	}
	return ret;
}

int ufsfbo_read_frag_level(struct ufsfbo_dev *fbo, char *buf)
{
	int ret = 0;
	uint8_t cdb[16];

	struct ufs_hba *hba = fbo->hba;
	struct scsi_sense_hdr sshdr = {};
	struct scsi_device *sdev = fbo->sdev_ufs_lu;

	unsigned long flags = 0;
	int para_len = 0;

	if(!fbo->fbo_lba_count)
		FBO_ERR_MSG("Invaild lba range count:%d",fbo->fbo_lba_count);
	if (fbo->is_ufs31)
		para_len = FBO_BODY_HEADER_SIZE + fbo->fbo_lba_count * FBO_BODY_ENTRY_SIZE;
	else
		para_len = FBO_HEADER_SIZE + FBO_BODY_HEADER_SIZE + fbo->fbo_lba_count * FBO_BODY_ENTRY_SIZE;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret){
		FBO_ERR_MSG("Get device fail");
		return ret;
	}

	hba->host->eh_noresume = 1;

	cdb[0] = READ_BUFFER;
	if (fbo->is_ufs31)
		cdb[1] = FBS_READ_LBA_RANGE_MODE;
	else {
		cdb[1] = FBO_READ_LBA_RANGE_MODE;
		cdb[2] = FBO_READ_LBA_RANGE_BUF_ID;
	}

	put_unaligned_be24(para_len, cdb + 6);

	ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, para_len, NULL, &sshdr, msecs_to_jiffies(15000), 0, 0, RQF_PM, NULL);
	if (ret)
		FBO_ERR_MSG("Read Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;

	return ret;
}

int ufsfbo_init_lba_buffer(struct ufsfbo_dev *fbo, const char *buf, char* lba_buf)
{
	int ret = 0;
	char *buf_ptr;
	char *lba;
	u64 lba_value_pre, lba_value_post;
	int len_index = 1, lba_index = 0;
	buf_ptr = kzalloc(strlen(buf) + 1, GFP_KERNEL);
	if (!buf_ptr) {
		FBO_ERR_MSG("Alloc buffer fail");
		ret = -ENOMEM;
		return ret;
	}
	memcpy(buf_ptr, buf, strlen(buf) + 1);

	/*create lba range buf send for device*/
	if (fbo->is_ufs31){
		lba_buf[1] = fbo->fbo_lba_count;
		lba_index = FBO_BODY_HEADER_SIZE;
	}
	else {
		lba_buf[5] = fbo->fbo_lba_count;
		lba_buf[6] = fbo->fbo_wholefile;
		lba_index = FBO_HEADER_SIZE + FBO_BODY_HEADER_SIZE;
	}

	while((lba = strsep(&buf_ptr, ",")) != NULL) {
		ret = kstrtou64(lba, 16, &lba_value_pre);
		if (ret) {
			FBO_ERR_MSG("invalid lba range value");
			ret = -ENODEV;
			goto out;
		}
		if (len_index % 2) {
			lba_value_post = lba_value_pre;
			put_unaligned_be32(lba_value_pre, lba_buf + lba_index);
		} else {
			if (lba_value_pre < lba_value_post) {
				ret = -ENODEV;
				FBO_ERR_MSG("invalid lba range length");
				goto out;
			}
			lba_value_pre = lba_value_pre - lba_value_post + 1;
			put_unaligned_be24(lba_value_pre, lba_buf + lba_index + 4);
			lba_index += FBO_BODY_ENTRY_SIZE;
		}
		len_index++;
	}
out:
	kfree(buf_ptr);
	return ret;
}

int ufsfbo_lba_list_write(struct ufsfbo_dev *fbo, const char *buf)
{
	int ret = 0;
	struct ufs_hba *hba = fbo->hba;
	struct scsi_device *sdev = fbo->sdev_ufs_lu;
	struct scsi_sense_hdr sshdr = {};
	char *lba_buf;
	unsigned long flags = 0;
	unsigned char cdb[10] = {0};
	int para_len = 0;

	if (fbo->is_ufs31)
		para_len = FBO_BODY_HEADER_SIZE + fbo->fbo_lba_count * FBO_BODY_ENTRY_SIZE;
	else
		para_len = FBO_HEADER_SIZE + FBO_BODY_HEADER_SIZE + fbo->fbo_lba_count * FBO_BODY_ENTRY_SIZE;

	lba_buf = kzalloc(FBO_LBA_RANGE_LENGTH, GFP_KERNEL);
	if (!lba_buf) {
		FBO_ERR_MSG("Alloc lba_buf fail");
		ret = -ENOMEM;
		return ret;
	}
	ret = ufsfbo_init_lba_buffer(fbo, buf, lba_buf);
	if (ret){
		FBO_ERR_MSG("init lba_buf fail");
		goto out;
	}
	/*create lba range buf send for device*/
	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret){
		FBO_ERR_MSG("Get device fail");
		goto out;
	}

	hba->host->eh_noresume = 1;

	cdb[0] = WRITE_BUFFER;
	if (fbo->is_ufs31)
		cdb[1] = FBS_WRITE_LBA_RANGE_MODE;
	else{
		cdb[1] = FBO_WRITE_LBA_RANGE_MODE;
		cdb[2] = FBO_WRITE_LBA_RANGE_BUF_ID;
	}

	put_unaligned_be24(para_len, cdb + 6);

	ret = scsi_execute(sdev, cdb, DMA_TO_DEVICE, lba_buf, para_len, NULL, &sshdr, msecs_to_jiffies(15000), 0, 0, RQF_PM, NULL);

	if (ret)
		/*check sense key*/
		FBO_ERR_MSG("Write Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
out:
	kfree(lba_buf);
	return ret;
}

int ufsfbo_get_fbo_desc_info(struct ufsfbo_dev *fbo)
{
	int ret;
	u8 *desc_buf;
	struct ufs_hba *hba = fbo->hba;

	desc_buf = kmalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ufsfbo_read_desc(hba, QUERY_DESC_IDN_FBO, 0, 0, desc_buf, QUERY_DESC_MAX_SIZE);
	if (ret) {
		FBO_ERR_MSG("Failed reading FBO Desc. ret(%d)", ret);
		goto out;
	}

	fbo->fbo_version = get_unaligned_be16(desc_buf + FBO_DESC_PARAM_VERSION);
	fbo->fbo_rec_lrs = get_unaligned_be32(desc_buf + FBO_DESC_PARAM_REC_LBA_RANGE_SIZE);
	fbo->fbo_max_lrs = get_unaligned_be32(desc_buf + FBO_DESC_PARAM_MAX_LBA_RANGE_SIZE);
	fbo->fbo_min_lrs = get_unaligned_be32(desc_buf + FBO_DESC_PARAM_MIN_LBA_RANGE_SIZE);
	fbo->fbo_max_lrc = desc_buf[FBO_DESC_PARAM_MAX_LBA_RANGE_CONUT];
	fbo->fbo_lra = get_unaligned_be16(desc_buf + FBO_DESC_PARAM_MAX_LBA_RANGE_ALIGNMENT);
out:
	kfree(desc_buf);
	return ret;
}

int ufsfbo_get_fbo_support_state(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->hba;
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u32 ext_ufs_feature;
	/* for UFS 3.1 */
	u32 support_mask = 0;
	u8 *desc_buf;
	int ret;

	if (fbo->vendor == UFS_HYNIX)
		support_mask = UFS_HYNIX_SUPPORT;
	if (fbo->vendor == UFS_MICRON)
		support_mask = UFS_MICRON_SUPPORT;
	if (fbo->vendor == UFS_WDC)
		support_mask = UFS_WDC_SUPPORT;
	FBO_INFO_MSG(fbo, "Support Mask(0x%x)", support_mask);

	desc_buf = kmalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ufsfbo_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, 0, desc_buf, QUERY_DESC_MAX_SIZE);
	if (ret) {
		FBO_ERR_MSG("Failed reading Device Desc. ret(%d)", ret);
		goto out;
	}

	ext_ufs_feature = get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (dev_info->wspecversion == 0x0310){
		fbo->is_ufs31 = 1;
		if (!(ext_ufs_feature & support_mask)){
			ret = -ENOTSUPP;
			FBO_INFO_MSG(fbo, "dExtendedUFSFeaturesSupport: UFS31 FBO not support");
			goto out;
		}
	} else if (dev_info->wspecversion > 0x0310) {
		if (!(ext_ufs_feature & UFS_DEV_FBO_SUP)){
			ret = -ENOTSUPP;
			FBO_INFO_MSG(fbo, "dExtendedUFSFeaturesSupport: FBO not support");
			goto out;
		}
	} else {
		ret = -ENOTSUPP;
		FBO_INFO_MSG(fbo, "dExtendedUFSFeaturesSupport: FBO not support");
		goto out;
	}

	fbo->fbo_support = 1;
	FBO_INFO_MSG(fbo, "dExtendedUFSFeaturesSupport: FBO support");

out:
	kfree(desc_buf);
	return ret;
}

static void ufsfbo_get_vendor_info(struct ufsfbo_dev *fbo)
{
	struct ufsfbo_standard_inquiry stdinq = {};
	struct ufs_hba *hba = fbo->hba;

	fbo->vendor = UFS_UNKNOWN;
	memcpy(&stdinq, hba->sdev_ufs_device->inquiry + 8, sizeof(stdinq));

	if (!strncmp((char *)stdinq.vendor_id, "SKhynix", strlen("SKhynix")))
		fbo->vendor = UFS_HYNIX;
	else if (!strncmp((char *)stdinq.vendor_id, "MICRON", strlen("MICRON")))
		fbo->vendor = UFS_MICRON;
	else if (!strncmp((char *)stdinq.vendor_id, "WDC", strlen("WDC")))
		fbo->vendor = UFS_WDC;

	FBO_INFO_MSG(fbo, "vendor:%d", fbo->vendor);
}

static int ufsfbo_get_fbs_attr_info(struct ufsfbo_dev *fbo)
{
	int ret;
	struct ufs_hba *hba = fbo->hba;

	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_REC_LRS, 0, 0, &fbo->fbo_rec_lrs);
	if (ret) {
		FBO_ERR_MSG("Get recommanded LBA range size failed");
		goto out;
	}
	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_MAX_LRS, 0, 0, &fbo->fbo_max_lrs);
	if (ret) {
		FBO_ERR_MSG("Get max LBA range size failed");
		goto out;
	}
	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_MIN_LRS, 0, 0, &fbo->fbo_min_lrs);
	if (ret) {
		FBO_ERR_MSG("Get min LBA range size failed");
		goto out;
	}
	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_MAX_LRC, 0, 0, &fbo->fbo_max_lrc);
	if (ret) {
		FBO_ERR_MSG("Get max LBA range count failed");
		goto out;
	}
	ret = ufsfbo_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_FBS_LRA, 0, 0, &fbo->fbo_lra);
	if (ret) {
		FBO_ERR_MSG("Get LBA range alignment failed");
		goto out;
	}

out:
	return ret;
}


void ufsfbo_init(struct ufs_hba *hba)
{
	struct ufsfbo_dev *fbo;
	int ret = 0;
	fbo = &hba->fbo;
	fbo->hba = hba;
	fbo->fbo_trigger = false;
	fbo->fbo_debug = false;
	fbo->block_suspend = false;
	fbo->is_ufs31 = 0;

	ufsfbo_get_vendor_info(fbo);

	ret = ufsfbo_get_fbo_support_state(fbo);
	if (ret) {
		FBO_ERR_MSG("NOT Support FBO. ret(%d)", ret);
		return;
	}

	if (fbo->is_ufs31){
		ret = ufsfbo_get_fbs_attr_info(fbo);
		if (ret) {
			FBO_ERR_MSG("Failed getting fbs info. ret(%d)", ret);
			return;
		}
	} else {
		/* JEDEC UFS4.0 Parameter*/
		ret = ufsfbo_get_fbo_desc_info(fbo);
		if (ret) {
			FBO_ERR_MSG("Failed getting fbo info. ret(%d)", ret);
			return;
		}
	}

	fbo->fbo_trigger_delay = FBO_TRIGGER_WORKER_DELAY_MS_DEFAULT;

	/* If HCI supports auto hibern8, UFS Driver use it default */
	if (ufshcd_is_auto_hibern8_supported(fbo->hba))
		fbo->is_auto_enabled = true;
	else
		fbo->is_auto_enabled = false;

	ret = ufsfbo_create_sysfs(fbo);
	if (ret) {
		FBO_ERR_MSG("Sysfs init fail, disabled fbo driver");
		ufsfbo_set_state(hba, FBO_FAILED);
		return;
	}

	FBO_INFO_MSG(fbo, "FBO Init and create sysfs finished");

	ufsfbo_set_state(hba, FBO_PRESENT);
}

