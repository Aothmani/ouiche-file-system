# PNL-2019

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

Pour cela on récupère le nombre d'inodes utilisés

# Déduplication de blocs

Pour dédupliquer les blocs, nous avons tout de suite cherché à éviter l'approche naïve consistant à comparer chaque bloc avec tous les autres. Ceci donnerait une complexité bien trop grande.

Nous avons décidé de hasher le contenu des blocs **utilisés** uniquement. Pour déterminer si un bloc est utilisé, nous avons récupéré les inodes utilisées via la `ifree_bitmap̀` contenue dans `ouichefs_sb_info`, et avons récupéré tous les blocs composant des fichiers. Nous avons choisi de hasher avec SHA256 pour ne comparer que 32 octets pour chaque bloc, au lieu des 4096 les composant. Nous avons ensuite construit une hashlist avec les hash calculés. Nous traitons les blocs complets "normalement", et le bloc restant n'est traité que partiellement (uniquement les données du fichier et non les résidus) grâce à la taille restante. L'algorithme est le suivant :
* Pour chaque bloc de données, nous le hashons, puis nous cherchons si le hash est contenu dans la hashlist
* S'il n'est pas présent, nous ajoutons une entrée dans la liste contenant le hash et le numéro du bloc hashé
* S'il est présent, il y a une collision, cela signifie qu'un autre bloc possède les mêmes données. Nous remplaçons alors dans son inode le numéro du bloc de données en double par le numéro du bloc déjà existant, puis le libérons. Les deux inodes possèdent désormais le même bloc de données.

Voici la structure représentant notre hash_list
```
static struct ouichefs_hashtable {
	struct list_head hash_list;
	char hash[SHA256_BLOCK_SIZE];
        uint32_t blockno;
};
```
`hash_list`: notre liste chaînée

`hash[]` : le hash calculé

`blockno`: le numéro de bloc possédant les données hashées

La recherche dans la hashlist a une complexité en *O(N)*, c'est pourquoi nous avons choisi cette structure.

# Copie sur écriture

Le but de cet exercice est d'implémenter un système de compteur sur les blocs. Si un bloc est utilisé par plus d'un fichier, et que l'on modifie un des fichiers, on ne souhaite pas répercuter les changements sur les autres. 

Notre première approche a été d'ajouter une `struct kref`pour chaque bloc. Nous avons donc ajouté un tableau de `struct kref`de taille `nr_blocks`,dans la structure `ouichefs_sb_info`.
