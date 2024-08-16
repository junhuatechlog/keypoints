


git clone --depth=1 https://github.com/raspberrypi/linux
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL=kernel8 bcm2711_defconfig

Kernel compile:
make -j3 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL=kernel8

ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL=kernel8 make -j4 zImage modules dtbs


junhuawa@junhuawa-VirtualBox:/media/junhuawa/bootfs$ file kernel.img
kernel.img: Linux kernel ARM64 boot executable Image, little-endian, 4K pages
junhuawa@junhuawa-VirtualBox:/media/junhuawa/bootfs$ file kernel-backup.img
kernel-backup.img: Linux kernel ARM boot executable zImage (little-endian)


ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL=kernel8 make -j4 dtbs

sudo apt install raspberrypi-kernel-headers


sudo env PATH=$PATH make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=mnt/root modules_install


echo > "8 4 1 7" > /proc/sys/kernel/printk

### Device tree
#### 反编译device tree文件生成dts文件

kernel_dir/script/dtc/dtc -I dtb -O dts -o xxx.dts xxx.dtb
#### uboot加载dtb查看
```python
    fdt addr
    fdt print
```
### kernel查看 printk

https://blog.csdn.net/xiaojunling/article/details/88955278



### linux 驱动设备和 dts 匹配过程
![](dt-match.png)

https://blog.csdn.net/sinat_30545941/article/details/85943787


### UART console setup

cmdline.txt:
no change: console=ttyAMA0,115200 console=ttyAMA0 root=PARTUUID=bb8aae7d-02 rootfstype=ext4 fsck.repair=yes rootwait loglevel=7

UART-USB converter:
connect GND, pin 8, pin 10 

remove quiet

config.txt:
```python
[all]
dtoverlay=disable-bt
enable_uart=1
```


bcm2711-gpio

CONFIG_PINCTRL_BCM2712=y
CONFIG_PINCTRL_BCM2835=y

### Remove a directory with Chinese char directory name
find . -inum 145837 -exec rm -rf {} \;
