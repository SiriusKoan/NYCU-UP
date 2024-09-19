
obj-m     += maze.o
ccflags-y += -DEXPORT_SYMTAB

all: maze test
	make -C ../dist/modulebuild M=$(PWD) modules

test: mazetest.c
	$(CROSS_COMPILE)gcc -o $@ $< -Wall -static

maze: maze.c
	$(CROSS_COMPILE)gcc -o $@ $< -Wall -static

clean:
	rm -f hello
	make -C ../dist/modulebuild M=$(PWD) clean

install: all
	mkdir -p ../rootfs/modules
	#cp hello ../rootfs/modules/maze
	#cp hellomod.ko ../rootfs/modules/maze.ko
	cp test ../rootfs/modules/mazetest
