# PNL-2019

# Setup ouichefs

On créé une image de 50Mo pour le filesystem:

```
dd if=/dev/zero of=ouichefs.img bs=1M count=50
mkfs.ouichfs ouichefs.img
```

Pour monter et démonter le system de fichier utilisez les scripts `mnt.sh` et
`unmnt.sh`.

L'image à monter doit se nommer `ouichefs.img` et se trouver à coter des scripts.

# Sysfs

Nous avons créé un `sysfs` pour pouvoir lister l'ensemble des block utilisé pour
chaque inode du système de fichier:

Utilisez cette commande après montage du module:

```
$ cat cat /sys/kernel/ouichefs/blocklst
$ [loop0]
	D [0]: 1 1635017076
	F [1]: 131 132
	F [2]: 131 132

```

Le résultat obtenu est une liste d'inode par disque monté.
Pour chaques inode nous avons son type `F` pour fichier `D` pour directory,
son numéro d'inode entre crochet et les numéros de blocks associés.

# Déduplication de blocs

Pour dédupliquer les blocs, nous avons tout de suite cherché à éviter l'approche naïve consistant à comparer chaque bloc avec tous les autres. Ceci donnerait une complexité bien trop grande( O(n²) ).


****************
Nous avons décidé de hasher le contenu des blocs **utilisés** uniquement. Pour déterminer si un bloc est utilisé, nous avons récupéré les inodes utilisées via la `ifree_bitmap̀` contenue dans `ouichefs_sb_info`, et avons récupéré tous les blocs composant des fichiers. Nous avons choisi de hasher avec SHA256 pour ne comparer que 32 octets pour chaque bloc, au lieu des 4096 les composant. Nous avons ensuite construit une hashlist avec les hash calculés. Nous traitons les blocs complets "normalement", et le bloc restant n'est traité que partiellement (uniquement les données du fichier et non les résidus) grâce à la taille restante. L'algorithme est le suivant :
**************


Pour ce faire:

* Pour chaque bloc de données, nous le hashons, puis nous cherchons si le hash est contenu dans la hashlist
* S'il n'est pas présent, nous ajoutons une entrée dans la liste contenant le hash et le numéro du bloc hashé
* S'il est présent, il y a une collision, cela signifie qu'un autre bloc possède les mêmes données. Nous remplaçons alors dans l'inode le numéro du bloc de données en double par le numéro du bloc déjà existant, puis le libérons. Les deux inodes possèdent désormais le même bloc de données.

Voici la structure représentant notre hash_list

```c
static struct ouichefs_hashtable {
	struct list_head hash_list;
	char hash[SHA256_BLOCK_SIZE];
        uint32_t blockno;
};
```

`hash_list`: notre liste chaînée

`hash[]` : le hash calculé

`blockno`: le numéro de bloc possédant les données hashées

La fonction de hashage utilisé est le sha256, qui renvoi une valeur de 256 bits facilement comparable.
La recherche dans la hashlist a une complexité en *O(N)*, nous avons choisis cette complexité car elle plus simple d'implémentation avec peu d'erreur et aucune (ou presques) collision entre les hash. Ainsi que le gain en performance.


# Copie sur ecriture

Le but de cet exercice est d'implémenter un système de compteur sur les blocs. Si un bloc est utilisé par plus d'un fichier, et que l'on modifie un des fichiers, on ne souhaite pas répercuter les changements sur les autres.

Nous avons tanté la création d'une structure en dure dans le super_block permettant d'enregistrer le nombre de block utilisé par chaques inodes.
Nous avons dans un premier temps tenter d'utiliser des structures kref dans le superblock. A cause de l'aspect dynamique (libération de la référence donc kfree) qui ne collais pas à notre implémentation
nous nous sommes alors tourné vers un simple tableau d'entiers et avons cherché à effectuer la libération du block à la main.


La difficulté de modifier le mkfs ne nous a empêché de teste notre implémentation.
