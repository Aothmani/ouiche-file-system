# Linux 4.19.3

Télécharger la version 4.19.3 de linux avec la commande suivante:

```
wget https://mirrors.edge.kernel.org/pub/linux/kernel/v4.x/linux-4.19.3.tar.gz
tar xvf linux-4.19.3.tar.gz
```

Compilation avec quelques paramètre changé, veuillez utiliser le fichier
de configuration fourni `kernel/.config`

```
cd linux-4.19.3
make -j 8
```

# OuiCheFS

```
cd ouichefs
make KERNELDIR=<path>
```

# Notre environnement de développement

Creation d'une image bootable:

```
qemu-img create archlinux.img 2g
```

Partitionner l'image en MBR avec une seule partition de 2Go avec `cfdisk`

```
cfdisk archlinux.img
```

Monter l'image et formater la partition:

```
sudo losetup --partscan --show --find archlinux.img
mkfs.ext4 /dev/loop0p1
```

Monter et installer archlinux sur la partition:

```
mkdir ./mnt
mount /dev/loop0p1 ./mnt
sudo pacstrap ./mnt base base-devel nano vim
genfstab -U ./mnt > ./mnt/etc/fstab
sudo arch-root /mnt
pacman -S grub os-prober
grub-install --target i386-pc /dev/loop0
exit
sudo umount /dev/loop0p1
sudo losetup -d /dev/loop0
```
