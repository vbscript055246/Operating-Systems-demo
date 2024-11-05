obj-m := kfetch_mod.o

KDIR = /lib/modules/$(shell uname -r)/build

# CFLAG = -L/lib/modules/$(shell uname -r)/build/lib -I/lib/modules/$(shell uname -r)/build/include

all:
	cp kfetch_mod_310555007.c kfetch_mod.c
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm kfetch_mod.c
	make -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod kfetch_mod.ko

unload:
	sudo rmmod kfetch_mod.ko

kfetch_mod.o: kfetch_mod.c
	gcc -c -o $@ $< 
