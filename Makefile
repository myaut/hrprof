LINUX := /root/linux-3.12.12/

filelist = hrprof.c
obj-m += hrprof.o

all:
	make -C $(LINUX) M=$(PWD) modules
	make -C $(LINUX) M=$(PWD) modules_install

tarball:
	tar cvf hrprof.tar $(filelist)
	gzip hrprof.tar

clean:
	make -C $(LINUX) M=$(PWD) clean

