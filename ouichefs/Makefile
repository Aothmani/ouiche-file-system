obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o sha256.o sysfs.o dedup.o

KERNELDIR ?= ../../linux-4.19.3

all:
	make -C $(KERNELDIR) M=$(PWD) modules

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean
