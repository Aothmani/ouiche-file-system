// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/list.h>
#include <linux/types.h>

#include "ouichefs.h"

static struct buffer_head *bh, *b;

int simple_hash(char *data, int size){
	int i;
      	if (size == OUICHEFS_BLOCK_SIZE) {
		char * hash[64];

		}
		return hash;
	} else {

	}
}

void deduplicate_blocks(struct super_block *sb)
{
	struct inode *inode
	struct ouichefs_sb_info *sb_info;
	struct ouichefs_inode_info *i_info;
	struct ouichefs_dir_block *dir_block;
	struct ouichefs_file_index_block *file_block;
	int i, j;
	int offset, ino, index_block, nr_used_inodes, blockno;
	unsigned long *i_free_bitmap;
	int blocks[1024];

	for (i = 0; i < 1024; i++)
		blocks[i] = 0;

	pr_info("before list\n");

	sb_info = OUICHEFS_SB(sb);
	i_free_bitmap = sb_info->i_free_bitmap;
	nr_used_inodes = sb_info->nr_inodes - sb_info->nr_free_inodes;

	offset = 0;
	for (i = 0; i < nr_used_inodes ; i++) {
		ino = find_next_zero_bit(i_free_bitmap, sb_info->nr_inodes, offset);
		inode = ouichefs_iget(sb, ino);
		i_info = OUICHEFS_INODE(inode);

		if(S_ISDIR(inode->imode)) {
		} else if (S_ISREG(inode->imode)) {
			bh = sb_bread(sb, i_info->index_block);
			file_block = (struct ouichefs_file_index_block *)bh->data;
			while (file_block->blocks[j] != 0){
				b = sb_bread(sb, file_block->blocks[j]);
				if (/* fct de recherche dans la liste de blocs search(b...) */) {

				} else {

				}
				j++;
			}
		} else {
			pr_warn("Error : wrong i_mode\n");
		}


	}
	/*
	list_for_each_entry(inode, &sb->s_inodes, i_lru){
		if (S_ISDIR(inode->i_mode))
			continue;

		info = OUICHEFS_INODE(inode);
		index_block = info->index_block;
		bh = sb_bread(sb, index_block);
		pr_info("Bloc index %d : ", index_block);

		i = 0;
		for (i = 0; i < inode->i_blocks; i++)
			pr_info("%d ", bh->b_data[i]);
 		pr_info("\n");
	}
	*/
}

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry = mount_bdev(fs_type, flags, dev_name, data,
			    ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	deduplicate_blocks(sb);
	kill_block_super(sb);
	brelse(bh);

	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};

static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto end;
	}

	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto end;
	}

	pr_info("module loaded\n");
end:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@lip6.fr>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
