#include "sysfs.h"

/*
 * Sysfs ouichefs
 */
struct kobject *ouichefs_group_kobj;



struct sios {
  int ret;
  char* buf;
  const char* name;
};

void show_inodes_of_superblock(struct super_block* sb, void* arg) {
  // Output struct
  struct sios* out = (struct sios*) arg;
  // Superblock
  struct ouichefs_sb_info* sbi;
  // inode
  struct inode* ino;
  struct ouichefs_inode_info* inodei;
  uint32_t i_ino;
  uint32_t nb_inodes = 0;
  // Block
  uint32_t i_blk;
  // Bufferhead
  struct buffer_head *bh_index;
  // file
  struct ouichefs_file_index_block *index;

  // Superblock id
  print_buf(out, "[%s]\n", sb->s_id);

  sbi = OUICHEFS_SB(sb); // get its true forme
  // Go throught all inodes
  for_each_clear_bit(i_ino, sbi->ifree_bitmap, sbi->nr_inodes) {
    // Get the inode
    ino = ouichefs_iget(sb, i_ino);
    inodei = OUICHEFS_INODE(ino);

    // Print if its a directory
    print_buf(out, "\t%c [%d]: ", S_ISDIR(ino->i_mode) ? 'D' : 'F' ,i_ino);
    // list all block of inode
    bh_index = sb_bread(sb, inodei->index_block);
    if(!bh_index)
        goto err_bhindex;

    index = (struct ouichefs_file_index_block *)bh_index->b_data;

    i_blk = 0;
    while(index->blocks[i_blk] != 0) {
      print_buf(out, "%d ", index->blocks[i_blk]);
      i_blk++;
    }

    brelse(bh_index);
    iput(ino);
    print_buf(out, "\n");
  }

  return;

err_bhindex:
  print_buf(out, "Error while getting buffer head !");
}

ssize_t ouichefs_show(struct kobject *kobj, struct kobj_attribute *attr,
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

/* Kobject sysfs */
struct kobj_attribute ouichefs_blocklst_attr = {
    .attr = { .name="blocklst", .mode=0444 },
    .show = ouichefs_show
};
