obj-m     +=sysfs-poll.o
KERNEL_DIR ?= $(HOME)/Documents/linux-raspberrypi

all:
	make -C $(KERNEL_DIR) \
	M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
	modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) ARCH=arm64 clean
