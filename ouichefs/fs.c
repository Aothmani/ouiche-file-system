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
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/bitops.h>

#include "ouichefs.h"

struct ouichefs_hashtable {
	char hash[SHA256_BLOCK_SIZE];
	struct list_head hash_list;
	struct ouichefs_block *block_head;
};

struct ouichefs_block {
	struct list_head block_list;
	int blockno;
};

static void hash_data(char * data, char *buf)
{
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, data, OUICHEFS_BLOCK_SIZE);
	sha256_final(&ctx, buf);
}

struct ouichefs_hashtable *create_new_hashtable(char *hash)
{
	struct ouichefs_hashtable *hashtable;
	struct ouichefs_block *block;

	hashtable = kmalloc(sizeof(struct ouichefs_hashtable), GFP_KERNEL);
	block = kmalloc(sizeof(struct ouichefs_block), GFP_KERNEL);
	strcpy(hashtable->hash, hash);
	hashtable->block_head = block;
	INIT_LIST_HEAD(&hashtable->block_head->block_list);

	return hashtable;
}

struct ouichefs_block *create_new_block(uint32_t blockno)
{
	struct ouichefs_block *new_block;

	new_block = kmalloc(sizeof(struct ouichefs_block), GFP_KERNEL);
	new_block->blockno = blockno;
	return new_block;
}

void deduplicate_blocks(struct super_block *sb)
{

	int i, j, added;
	int offset, ino, index_block, nr_used_inodes, blockno;
	unsigned long *i_free_bitmap;
	char hash[SHA256_BLOCK_SIZE];
	
	struct inode *inode;
	struct ouichefs_sb_info *sb_info;
	struct ouichefs_inode_info *i_info;
	struct ouichefs_file_index_block *file_block;
	struct ouichefs_hashtable *hashtable;
	static struct buffer_head *bh, *b;

	hashtable = kmalloc(sizeof(struct ouichefs_hashtable), GFP_KERNEL);
	INIT_LIST_HEAD(&hashtable->hash_list);
	memset(hashtable->hash, 0, sizeof(hashtable->hash));

	sb_info = OUICHEFS_SB(sb);
	i_free_bitmap = sb_info->ifree_bitmap;
	nr_used_inodes = sb_info->nr_inodes - sb_info->nr_free_inodes;
	
	offset = 0;
	pr_info("Before for : nr_used_inodes = %d\n", nr_used_inodes);
	for_each_clear_bit(ino, i_free_bitmap, sb_info->nr_inodes) {
		pr_info("ino = %d\n", ino);
		inode = ouichefs_iget(sb, ino);
		i_info = OUICHEFS_INODE(inode);
		if (S_ISREG(inode->i_mode)) {
			pr_info("isreg\n");
			bh = sb_bread(sb, i_info->index_block);
			file_block = (struct ouichefs_file_index_block *)bh->b_data;
			pr_info("number of blocks of file inode = %d\n", inode->i_blocks);
			if (inode->i_blocks > 2){
				for (j = 0; j < inode->i_blocks - 2; j++){
					b = sb_bread(sb, file_block->blocks[j]);
					hash_data(b->b_data, hash);
				
					struct ouichefs_hashtable *h;
					added = 0;
					list_for_each_entry(h, &hashtable->hash_list, hash_list){
						pr_info("Hash tested : %s = %s ?\n", h->hash, hash);
						if (strcmp(h->hash, hash) == 0){
							pr_info("Same hash found : %s = %s\n", h->hash, hash);
							struct ouichefs_block *new_block;
							new_block = create_new_block(file_block->blocks[j]);
							list_add_tail(&new_block->block_list, &h->block_head->block_list);
							added = 1;
							break;
						}
					}
					if (added == 0){
						struct ouichefs_hashtable *new_hashtable;
						struct ouichefs_block *new_block;
					
						new_hashtable =	create_new_hashtable(hash); /* hash des données */
						new_block = create_new_block(file_block->blocks[j]); /* numero du bloc */
						list_add_tail(&new_hashtable->hash_list, &hashtable->hash_list);
						list_add_tail(&new_block->block_list, &new_hashtable->block_head->block_list);
					}

				}
			}
		}
	}
	//	kfree(hashtable);
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
	//	brelse(bh);

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
