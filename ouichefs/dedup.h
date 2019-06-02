#ifndef MODULE_DEDUP
#define MODULE_DEDUP

#include <linux/list.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <asm/bitops.h>

#include "ouichefs.h"
#include "bitmap.h"
#include "sha256.h"

extern void deduplicate(struct super_block *sb);

#endif // MODULE_DEDUP
