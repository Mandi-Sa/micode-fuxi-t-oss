/*
 * mi_ufsfbo.h
 *
 * Created on: 2021-09-17
 *
 * Authors:
 *	lijiaming <lijiaming3@xiaomi.com>
 */

#ifndef _UFSFBO_H_
#define _UFSFBO_H_

#include <linux/sysfs.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <linux/types.h>
#include <asm/unaligned.h>


#define FBO_INFO_MSG(fbo, msg, args...)		do {if (fbo->fbo_debug) \
											pr_info("[info]%s:%d:" msg "\n", __func__, __LINE__, ##args);} while(0)

#define FBO_ERR_MSG(msg, args...)		pr_err("[err]%s:%d:" msg "\n", \
					       __func__, __LINE__, ##args)
#define FBO_WARN_MSG(msg, args...)		pr_warn("[warn]%s:%d:" msg "\n", \
					       __func__, __LINE__, ##args)

#define FBO_TRIGGER_WORKER_DELAY_MS_DEFAULT	2000
#define FBO_TRIGGER_WORKER_DELAY_MS_MIN		100
#define FBO_TRIGGER_WORKER_DELAY_MS_MAX		10000

#define FBO_AUTO_HIBERN8_DISABLE  (FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 0) | \
				   FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3))
#define FBO_LBA_RANGE_LENGTH (4*1024)

#define FBO_READ_LBA_RANGE_MODE 0x02
#define FBO_READ_LBA_RANGE_BUF_ID 0x02
#define FBO_READ_LBA_RANGE_BUF_OFFSET 0x00
#define FBO_WRITE_LBA_RANGE_MODE 0x02
#define FBO_WRITE_LBA_RANGE_BUF_ID 0x01
#define FBO_WRITE_LBA_RANGE_BUF_OFFSET 0x00

#define FBO_HEADER_SIZE 4
#define FBO_BODY_HEADER_SIZE 8
#define FBO_BODY_ENTRY_SIZE 8

#define QUERY_ATTR_IDN_FBO_CONTROL 0x31   /*enable start stop*/
#define QUERY_ATTR_IDN_FBO_LEVEL_EXE 0X32
#define QUERY_ATTR_IDN_FBO_PROG_STATE 0x33

enum UFSFBO_STATE {
	FBO_NEED_INIT = 0,
	FBO_PRESENT = 1,
	FBO_FAILED = -2,
	FBO_RESET = -3,
};

enum FBO_PROG_STATE {
	FBO_PROG_IDLE      = 0x0,
	FBO_PROG_ON_GOING    = 0x1,
	FBO_PROG_ANALYSIS_COMPLETE  = 0x2,
	FBO_PROG_OPTIMIZATION_COMPLETE      = 0x3,
	FBO_PROG_INTERNAL_ERR = 0xff,
};

/* FOR UFS3.1 FBS */
enum UFSFBS_OPS {
	UFSFBS_OPS_HOST_NA      = 0x0,
	UFSFBS_OPS_DEVICE_NA    = 0x1,
	UFSFBS_OPS_PROGRESSING  = 0x2,
	UFSFBS_OPS_SUCCESS      = 0x3,
	UFSFBS_OPS_PRE_MATURELY = 0x4,
	UFSFBS_OPS_HOST_LBA_NA  = 0x5,
	UFSFBS_OPS_INTERNAL_ERR = 0xff,
};
/* FOR UFS3.1 FBS */
enum UFSFBS_VENDOR{
	UFS_HYNIX,
	UFS_MICRON,
	UFS_WDC,
	UFS_UNKNOWN,
};
/* FOR UFS3.1 FBS */
enum {
	UFS_HYNIX_SUPPORT	= BIT(10),
	UFS_MICRON_SUPPORT	= BIT(30),
	/* NO Support Bit In WDC UFS3.1 */
	UFS_WDC_SUPPORT	= 0xFFFFFFFF,
};
/* FOR UFS3.1 FBS */
struct ufsfbo_standard_inquiry {
	uint8_t vendor_id[8];
	uint8_t product_id[16];
	uint8_t product_rev[4];
};
/* FOR UFS3.1 FBS */
#define FBS_READ_LBA_RANGE_MODE 0x1d
#define FBS_WRITE_LBA_RANGE_MODE 0x1d
#define QUERY_ATTR_IDN_FBS_REC_LRS 0x33
#define QUERY_ATTR_IDN_FBS_MAX_LRS 0X34
#define QUERY_ATTR_IDN_FBS_MIN_LRS 0X35
#define QUERY_ATTR_IDN_FBS_MAX_LRC 0X36
#define QUERY_ATTR_IDN_FBS_LRA 0X37
#define QUERY_ATTR_IDN_FBS_LEVEL_EXE 0X38
#define QUERY_ATTR_IDN_FBS_FLC_OPS 0X39
#define QUERY_ATTR_IDN_FBS_DEFRAG_OPS 0X3A
#define QUERY_ATTR_IDN_FBS_FLC_ENABLE 0X13
#define QUERY_ATTR_IDN_FBS_DEFRAG_ENABLE 0x14
#define QUERY_ATTR_IDN_MICRON_FBS_FLC_ENABLE 0X14
#define QUERY_ATTR_IDN_MICRON_FBS_DEFRAG_ENABLE 0x15
#define FBS_BODY_HEADER_SIZE 8
#define FBS_BODY_ENTRY_SIZE 8

enum {
	FBO_AH8_DISABLE = 0,
	FBO_AH8_ENABLE  = 1,
};

struct ufsfbo_dev {
	struct ufs_hba *hba;
	struct scsi_device *sdev_ufs_lu;
	unsigned int fbo_trigger;   /* default value is false */
	struct delayed_work fbo_trigger_work;
	unsigned int fbo_trigger_delay;
	u32 ahit;			/* to restore ahit value */
	bool is_auto_enabled;
	bool block_suspend;

	atomic_t fbo_state;
	/* for sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufsfbo_sysfs_entry *sysfs_entries;

	int fbo_lba_count;
	u8  vendor;
	u8  is_ufs31;
	u8  fbo_support;
	u16 fbo_version;
	u32 fbo_rec_lrs;
	u32 fbo_max_lrs;
	u32 fbo_min_lrs;
	int fbo_max_lrc;
	int fbo_lra;
	/* for debug */
	bool fbo_debug;
	bool fbo_wholefile;
	unsigned long fbo_err_cnt;
	unsigned long fbo_retry_cnt;
};

struct ufsfbo_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufsfbo_dev *fbo, char *buf);
	ssize_t (*store)(struct ufsfbo_dev *fbo, const char *buf, size_t count);
};

int ufsfbo_get_fbo_prog_state(struct ufsfbo_dev *fbo, int *prog_state);
int ufsfbo_operation_control(struct ufsfbo_dev *fbo, int *val);
int ufsfbo_get_exe_level(struct ufsfbo_dev *fbo, int *frag_exe_level);
int ufsfbo_set_exe_level(struct ufsfbo_dev *fbo, int *frag_exe_level);
int ufsfbo_lba_list_write(struct ufsfbo_dev *fbo, const char *buf);
int ufsfbo_read_frag_level(struct ufsfbo_dev *fbo, char *buf);
int ufsfbo_get_fbo_desc_info(struct ufsfbo_dev *fbo);
int ufsfbo_get_fbo_support_state(struct ufsfbo_dev *fbo);

int ufsfbo_get_state(struct ufs_hba *hba);
void ufsfbo_set_state(struct ufs_hba *hba, int state);
void ufsfbo_init(struct ufs_hba *hba);
void ufsfbo_reset(struct ufs_hba *hba);
void ufsfbo_reset_host(struct ufs_hba *hba);
void ufsfbo_remove(struct ufs_hba *hba);
int ufsfbo_is_not_present(struct ufsfbo_dev *fbo);
void ufsfbo_block_enter_suspend(struct ufsfbo_dev *fbo);
void ufsfbo_allow_enter_suspend(struct ufsfbo_dev *fbo);
void ufsfbo_auto_hibern8_enable(struct ufsfbo_dev *fbo, unsigned int val);

/* for ufs 3.1 fbs*/
int ufsfbs_get_level_check_ops(struct ufsfbo_dev *fbo, int *level_ops);
int ufsfbs_get_ops(struct ufsfbo_dev *fbo, int *defrag_ops);
int ufsfbs_frag_level_check_enable(struct ufsfbo_dev *fbo, bool level_check_enable);
int ufsfbs_defrag_enable(struct ufsfbo_dev *fbo, bool defrag_enable);

#endif /* _MI_FBO_H_ */
