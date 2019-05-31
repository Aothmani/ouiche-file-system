# PNL-2019

Fonctions utiles :

Monter/démonter un FS : (Documentation/filesystem/vfs.txt l.89)

```c
#include <linux/fs.h>

   extern int register_filesystem(struct file_system_type *);
   extern int unregister_filesystem(struct file_system_type *);

// Avec struct file_system_type qui décrit notre fs (TRES IMPORTANT : voir l.108 pour les détails)

struct file_system_type {
	const char *name;
	int fs_flags;
        struct dentry *(*mount) (struct file_system_type *, int,
                       const char *, void *);
        void (*kill_sb) (struct super_block *);
        struct module *owner;
        struct file_system_type * next;
        struct list_head fs_supers;
	struct lock_class_key s_lock_key;
	struct lock_class_key s_umount_key;
};


struc super_operations rassemblant les fonctions de manipulation d'un super_block

struct super_operations {
        struct inode *(*alloc_inode)(struct super_block *sb);
        void (*destroy_inode)(struct inode *);

        void (*dirty_inode) (struct inode *, int flags);
        int (*write_inode) (struct inode *, int);
        void (*drop_inode) (struct inode *);
        void (*delete_inode) (struct inode *);
        void (*put_super) (struct super_block *);
        int (*sync_fs)(struct super_block *sb, int wait);
        int (*freeze_fs) (struct super_block *);
        int (*unfreeze_fs) (struct super_block *);
        int (*statfs) (struct dentry *, struct kstatfs *);
        int (*remount_fs) (struct super_block *, int *, char *);
        void (*clear_inode) (struct inode *);
        void (*umount_begin) (struct super_block *);

        int (*show_options)(struct seq_file *, struct dentry *);

        ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);
        ssize_t (*quota_write)(struct super_block *, int, const char *, size_t, loff_t);
	int (*nr_cached_objects)(struct super_block *);
	void (*free_cached_objects)(struct super_block *, int);
};
```

# Setup ouichefs

On créé une image de 50Mo pour le filesystem:

```
dd if=/dev/zero of=ouichefs.img bs=1M count=50
mkfs.ouichfs ouichefs.img
```

Pour monter et démonter le system de fichier utilisez les scripts `mnt.sh` et
`unmnt.sh`

# Sysfs

Nous avons créé un `sysfs` pour pouvoir lister l'ensemble des block utilisé pour
chaque inode du système de fichier:

Pour cela on récupère le nombre de d'inode utilisé
