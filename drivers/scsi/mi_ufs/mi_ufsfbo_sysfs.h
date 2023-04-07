/*
 * mi_ufsfbo_sysfs.h
 *
 * Created on: 2021-09-17
 *
 * Authors:
 *	lijiaming <lijiaming3@xiaomi.com>
 */
#ifndef DRIVERS_SCSI_UFS_MI_UFSFBO_SYSFS_H_
#define DRIVERS_SCSI_UFS_MI_UFSFBO_SYSFS_H_

int ufsfbo_create_sysfs(struct ufsfbo_dev *fbo);
void ufsfbo_remove_sysfs(struct ufsfbo_dev *fbo);

#endif /* _MI_FBO_SYSFS_H_ */
