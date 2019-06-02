#include "dedup.h"

/* Structures de données */

/* Au début on a voulu une hashtable mais
 * une liste est plus pratique vu le nombre d'inode
 */
static struct ouichefs_hashtable {
	struct list_head hash_list;
	char hash[SHA256_BLOCK_SIZE];
        uint32_t blockno;
};
static struct ouichefs_hashtable hashtable;

/* File system variables */
static struct super_block *sb;
static struct ouichefs_sb_info *sb_info;

/* Fonctions */

/* Fonction de hashage SHA256 */
static void hash_data(char * data, char *hash, size_t len)
{
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, data, len);
	sha256_final(&ctx, hash);
}

static struct ouichefs_hashtable *create_new_hashtable(char *hash, uint32_t blockno)
{
	struct ouichefs_hashtable *hashtable;
	hashtable = kmalloc(sizeof(struct ouichefs_hashtable), GFP_KERNEL);

	strncpy(hashtable->hash, hash, SHA256_BLOCK_SIZE);
        hashtable->blockno = blockno;

	return hashtable;
}

/* Déduplication d'un bloc */

static void deduplicate_blocks(struct super_block *sb,
    uint32_t source,
    uint32_t* target) {
        // Duplicate found
        struct ouichefs_sb_info* sbi;
        sbi = OUICHEFS_SB(sb);

	(sbi->ref_table[source])++;
	
        // free block from inode
        put_block(sbi, *target);
        *target = source;

        pr_info("Block dédupliqué\n");
}

static void treat_block(uint32_t j, struct ouichefs_file_index_block* file_block, size_t len) {
        int added;
	char hash[SHA256_BLOCK_SIZE];

        static struct buffer_head *b;

        pr_info("j: %d\n", j);
        b = sb_bread(sb, file_block->blocks[j]);
        hash_data(b->b_data, hash, len);

        struct ouichefs_hashtable *h;
        added = 0;
        list_for_each_entry(h, &hashtable.hash_list, hash_list){
                pr_info("Hash tested : %x = %x ?\n", h->hash[0], hash[0]);
                if (strncmp(h->hash, hash, SHA256_BLOCK_SIZE) == 0){
                        pr_info("Same hash found : %x = %x\n", h->hash[0], hash[0]);
                        deduplicate_blocks(sb, h->blockno, &file_block->blocks[j]);
                        added = 1;
                        break;
                }
        }

        // Changer pour check la fin de liste
        if (added == 0){
                struct ouichefs_hashtable *new_hashtable;

                new_hashtable =	create_new_hashtable(hash, file_block->blocks[j]); /* hash des données */
                list_add_tail(&new_hashtable->hash_list, &hashtable.hash_list);
        }
        brelse(b);
}

/* Treatement de l'inode */
static void treat_inode(uint32_t ino) {

	int i, j, added;
        int blockno;
        int index_block;
        uint32_t inode_isize;

        struct inode* inode;
	struct ouichefs_inode_info *i_info;
	struct ouichefs_file_index_block *file_block;
	static struct buffer_head *bh;


        pr_info("ino = %d\n", ino);
        inode = ouichefs_iget(sb, ino);
        i_info = OUICHEFS_INODE(inode);
        inode_isize = inode->i_size;
        if (S_ISREG(inode->i_mode)) {
                pr_info("isreg\n");
                bh = sb_bread(sb, i_info->index_block);
                file_block = (struct ouichefs_file_index_block *)bh->b_data;
                pr_info("number of blocks of file inode = %d\n", inode->i_blocks);
                for (j = 0; j < inode->i_blocks - 2; j++){
                    /* Traitement du block */
                    inode_isize -= OUICHEFS_BLOCK_SIZE;
                    treat_block(j, file_block, SHA256_BLOCK_SIZE);
                    mark_buffer_dirty(bh);
                }
                treat_block(j, file_block, inode_isize);
                mark_buffer_dirty(bh);
                /*
                b = sb_bread(sb, file_block->blocks[j]);
                hash_data(b->b_data, hash, inode_isize);
                struct ouichefs_hashtable *h;
                added = 0;
                list_for_each_entry(h, &hashtable.hash_list, hash_list){
                        pr_info("Hash tested : %x = %x ?\n", h->hash[0], hash[0]);
                        if (strncmp(h->hash, hash, SHA256_BLOCK_SIZE) == 0){
                                pr_info("Same hash found : %x = %x\n", h->hash[0], hash[0]);
                                deduplicate_blocks(sb, h->blockno, &file_block->blocks[j]);
                                mark_buffer_dirty(bh);
                                added = 1;
                                break;
                        }
                }
                if (added == 0){
                        struct ouichefs_hashtable *new_hashtable;
                        struct ouichefs_block *new_block;

                        new_hashtable =	create_new_hashtable(hash, file_block->blocks[j]);
                        list_add_tail(&new_hashtable->hash_list, &hashtable.hash_list);
                }
                brelse(b);
                */
                brelse(bh);
        }
        iput(inode);
}


void deduplicate(struct super_block *sb_)
{
	int offset, ino, nr_used_inodes;
	unsigned long *i_free_bitmap;

        /* Initialize hashtable (hashlist) */
	INIT_LIST_HEAD(&hashtable.hash_list);
	memset(hashtable.hash, 0, sizeof(hashtable.hash));


        sb = sb_;
	sb_info = OUICHEFS_SB(sb);
	i_free_bitmap = sb_info->ifree_bitmap;
	nr_used_inodes = sb_info->nr_inodes - sb_info->nr_free_inodes;

	offset = 0;
	pr_info("Before for : nr_used_inodes = %d\n", nr_used_inodes);
	for_each_clear_bit(ino, i_free_bitmap, sb_info->nr_inodes)
          treat_inode(ino);
}
