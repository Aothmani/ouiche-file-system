// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define print_buf(out, str, ...) out->ret += sprintf(out->buf+out->ret, str, ##__VA_ARGS__ )

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/buffer_head.h>
#include <linux/kobject.h>
#include <linux/buffer_head.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/bitops.h>

#include "ouichefs.h"
#include "bitmap.h"

/*
 * Sysfs ouichefs
 */
static struct kobject *ouichefs_group_kobj;
static struct file_system_type ouichefs_file_system_type;

struct sios {
  int ret;
  char* buf;
  const char* name;
};

static void show_inodes_of_superblock(struct super_block* sb, void* arg) {
  // Output struct
  struct sios* out = (struct sios*) arg;
  // Superblock
  struct ouichefs_sb_info* sb_info;
  // inode
  struct inode* ino;
  struct ouichefs_inode_info* oui_ino;
  uint32_t i_ino;
  uint32_t nb_inodes = 0;
  // Block
  uint32_t i_blk;
  // Bufferhead
  struct buffer_head *bh_index;
  // file
  struct ouichefs_file_index_block *index;

  // Get sb_info
  sb_info = (struct ouichefs_sb_info*) sb->s_fs_info;

  // Superblock id
  print_buf(out, "[%s]\n", sb->s_id);

  // Go throught all inodes
  nb_inodes = sb_info->nr_inodes - sb_info->nr_free_inodes;
  for(i_ino = 0; i_ino < nb_inodes; ++i_ino) {
    // Get the inode
    ino = ouichefs_iget(sb, i_ino);
    oui_ino = OUICHEFS_INODE(ino); // get its true forme

    // Print if its a directory
    print_buf(out, "\t%c [%d]: ", S_ISDIR(ino->i_mode) ? 'D' : 'F' ,i_ino);


    // list all block of inode
    bh_index = sb_bread(sb, oui_ino->index_block);
    if(!bh_index)
        goto err_bhindex;

    index = (struct ouichefs_file_index_block *)bh_index->b_data;

    i_blk = 0;
    while(index->blocks[i_blk] != 0) {
      print_buf(out, "%d ", index->blocks[i_blk]);
      i_blk++;
    }

    iput(ino);
    brelse(bh_index);
    print_buf(out, "\n");
  }

  return;

err_bhindex:
  print_buf(out, "Error while getting buffer head !");
}

static ssize_t ouichefs_show(struct kobject *kobj, struct kobj_attribute *attr,
    char *buf) {

  struct sios out = {
    .ret = 0,
    .buf = buf,
  };
  iterate_supers_type(
      &ouichefs_file_system_type,
      show_inodes_of_superblock,
      &out
  );

  return out.ret;
}

struct ouichefs_hashtable {
	char hash[SHA256_BLOCK_SIZE];
	struct list_head hash_list;
	struct ouichefs_block *block_head;
};

struct ouichefs_block {
	struct list_head block_list;
	int blockno;
};

static void hash_data(char * data, char *buf, size_t len)
{
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, data, len);
	sha256_final(&ctx, buf);
}

struct ouichefs_hashtable *create_new_hashtable(char *hash)
{
	struct ouichefs_hashtable *hashtable;
	struct ouichefs_block *block;

	hashtable = kmalloc(sizeof(struct ouichefs_hashtable), GFP_KERNEL);
	block = kmalloc(sizeof(struct ouichefs_block), GFP_KERNEL);
	strncpy(hashtable->hash, hash, SHA256_BLOCK_SIZE);
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
        uint32_t inode_isize;

	struct inode *inode;
	struct ouichefs_sb_info *sb_info;
	struct ouichefs_inode_info *i_info;
	struct ouichefs_file_index_block *file_block;
	static struct buffer_head *bh, *b;
        struct ouichefs_hashtable hashtable;

	INIT_LIST_HEAD(&hashtable.hash_list);
	memset(hashtable.hash, 0, sizeof(hashtable.hash));

	sb_info = OUICHEFS_SB(sb);
	i_free_bitmap = sb_info->ifree_bitmap;
	nr_used_inodes = sb_info->nr_inodes - sb_info->nr_free_inodes;

	offset = 0;
	pr_info("Before for : nr_used_inodes = %d\n", nr_used_inodes);
	for_each_clear_bit(ino, i_free_bitmap, sb_info->nr_inodes) {
		pr_info("ino = %d\n", ino);
		inode = ouichefs_iget(sb, ino);
		i_info = OUICHEFS_INODE(inode);
                inode_isize = inode_isize;
		if (S_ISREG(inode->i_mode)) {
			pr_info("isreg\n");
			bh = sb_bread(sb, i_info->index_block);
			file_block = (struct ouichefs_file_index_block *)bh->b_data;
			pr_info("number of blocks of file inode = %d\n", inode->i_blocks);
                        for (j = 0; j < inode->i_blocks - 2; j++){
                                pr_info("j: %d\n", j);
                                b = sb_bread(sb, file_block->blocks[j]);
                                hash_data(b->b_data, hash, OUICHEFS_BLOCK_SIZE);
                                inode_isize -= OUICHEFS_BLOCK_SIZE;

                                struct ouichefs_hashtable *h;
                                added = 0;
                                list_for_each_entry(h, &hashtable.hash_list, hash_list){
                                        pr_info("Hash tested : %x = %x ?\n", h->hash[0], hash[0]);
                                        if (strncmp(h->hash, hash, SHA256_BLOCK_SIZE) == 0){
                                                pr_info("Same hash found : %x = %x\n", h->hash[0], hash[0]);
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
                                        list_add_tail(&new_hashtable->hash_list, &hashtable.hash_list);
                                        list_add_tail(&new_block->block_list, &new_hashtable->block_head->block_list);
                                }
                        }
                        b = sb_bread(sb, file_block->blocks[j]);
                        hash_data(b->b_data, hash, inode_isize);
                        struct ouichefs_hashtable *h;
                        added = 0;
                        list_for_each_entry(h, &hashtable.hash_list, hash_list){
                                pr_info("Hash tested : %x = %x ?\n", h->hash[0], hash[0]);
                                if (strncmp(h->hash, hash, SHA256_BLOCK_SIZE) == 0){
                                        pr_info("Same hash found : %x = %x\n", h->hash[0], hash[0]);
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
                                list_add_tail(&new_hashtable->hash_list, &hashtable.hash_list);
                                list_add_tail(&new_block->block_list, &new_hashtable->block_head->block_list);
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
	if (IS_ERR(dentry)){
		pr_err("'%s' mount failure\n", dev_name);
        }
	else {
		pr_info("'%s' mount success\n", dev_name);
                        }

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

/* Kobject sysfs */
static struct kobj_attribute ouichefs_blocklst_attr = {
    .attr = { .name="blocklst", .mode=0444 },
    .show = ouichefs_show
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

        // Register sysfs group
        ouichefs_group_kobj = kobject_create_and_add("ouichefs", kernel_kobj);
        if(!ouichefs_group_kobj) {
                pr_err("kobject_create_and_add() failed\n");
                goto end;
        }
        ret = sysfs_create_file(ouichefs_group_kobj,
              &ouichefs_blocklst_attr.attr);
        if(ret)
            pr_err("Error while sysfs_create_file()");

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

        // remove sysfs
        kobject_put(ouichefs_group_kobj);

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@lip6.fr>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
