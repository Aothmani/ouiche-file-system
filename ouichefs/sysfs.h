#ifndef MODULE_SYSFS
#define MODULE_SYSFS

#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"

#define print_buf(out, str, ...) out->ret += sprintf(out->buf+out->ret, str, ##__VA_ARGS__ )

extern struct kobject *ouichefs_group_kobj;
extern struct kobj_attribute ouichefs_blocklst_attr;
extern struct file_system_type ouichefs_file_system_type;

#endif // MODULE_SYSFS
