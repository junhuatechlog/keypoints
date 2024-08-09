obj-m     +=int_rpi4_key_tasklet.o
KERNEL_DIR ?= $(HOME)/pi_kernel/linux

all:
	make -C $(KERNEL_DIR) \
	M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
	modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) ARCH=arm64 clean
