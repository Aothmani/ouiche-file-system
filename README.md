# PNL-2019

* Ulysse FONTAINE (3520203)
* Anthony OTHMANI (3200072)

# Setup ouichefs

On créé une image de 50Mo pour le filesystem:

```
dd if=/dev/zero of=ouichefs.img bs=1M count=50
mkfs.ouichfs ouichefs.img
```

Pour monter et démonter le système de fichier, utilisez les scripts `mnt.sh` et
`unmnt.sh`.

L'image à monter doit se nommer `ouichefs.img` et se trouver à côté des scripts.

# Sysfs

Nous avons créé un `sysfs` pour pouvoir lister l'ensemble des blocs utilisés pour
chaque inode du système de fichier:

Utilisez cette commande après montage du module:

```
$ cat cat /sys/kernel/ouichefs/blocklst
$ [loop0]
	D [0]: 1 1635017076
	F [1]: 131 132
	F [2]: 131 132

```

Le résultat obtenu est une liste d'inodes par disque monté.
Pour chaque inode, nous avons son type `F` pour fichier `D` pour dossier,
son numéro d'inode entre crochets et les numéros de bloc>>>>>>> 3bbb859973ad9b3eaa6cf5834171bc9b153e17d1>>>>>>> 3bbb859973ad9b3eaa6cf5834171bc9b153e17d1>>>>>>> 3bbb859973ad9b3eaa6cf5834171bc9b153e17d1>>>>>>> 3bbb859973ad9b3eaa6cf5834171bc9b153e17d1>>>>>>> 3bbb859973ad9b3eaa6cf5834171bc9b153e17d1s associés.

# Déduplication de blocs

Pour dédupliquer les blocs, nous avons tout de suite cherché à éviter l'approche naïve consistant à comparer chaque bloc avec tous les autres. Ceci donnerait une complexité bien trop grande( O(n²) ).


****************
Nous avons décidé de hasher le contenu des blocs **utilisés** uniquement. Pour déterminer si un bloc est utilisé, nous avons récupéré les inodes utilisées via la `ifree_bitmap̀` contenue dans `ouichefs_sb_info`, et avons récupéré tous les blocs composant des fichiers. Nous avons choisi de hasher avec SHA256 pour ne comparer que 32 octets pour chaque bloc, au lieu des 4096 les composant. Nous avons ensuite construit une hashlist avec les hash calculés. Nous traitons les blocs complets "normalement", et le bloc restant n'est traité que partiellement (uniquement les données du fichier et non les résidus) grâce à la taille restante. L'algorithme est le suivant :
**************


Pour ce faire:

* Pour chaque bloc de données, nous le hashons, puis nous cherchons si le hash est contenu dans la hashlist
* S'il n'est pas présent, nous ajoutons une entrée dans la liste contenant le hash et le numéro du bloc hashé
* S'il est présent, il y a une collision, cela signifie qu'un autre bloc possède les mêmes données. Nous remplaçons alors dans l'inode le numéro du bloc de données en double par le numéro du bloc déjà existant, puis le libérons. Les deux inodes possèdent désormais le même bloc de données.


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


La fonction de hashage utilisée est le sha256, qui renvoie une valeur de 256 bits facilement comparables.
La recherche dans la hashlist a une complexité en *O(N)*. Nous avons choisi cette complexité car elle plus simple d'implémentation avec peu d'erreurs et aucune (ou presque) collision entre les hash. Ainsi que le gain en performance.


# Copie sur ecriture

Le but de cet exercice est d'implémenter un système de compteur sur les blocs. Si un bloc est utilisé par plus d'un fichier, et que l'on modifie un des fichiers, on ne souhaite pas répercuter les changements sur les autres.

Nous avons tenté la création d'une structure en dur dans le super_block permettant d'enregistrer le nombre de blocs utilisés par chaques inode.
Nous avons dans un premier temps tenté d'utiliser des structures kref dans le superblock. A cause de l'aspect dynamique (libération de la référence donc kfree) qui ne collait pas à notre implémentation nous nous sommes alors tourné vers un simple tableau d'entiers et avons cherché à effectuer la libération du bloc à la main.


La difficulté de modifier le mkfs ne nous a empêché de teste notre implémentation.
