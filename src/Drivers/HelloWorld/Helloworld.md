编译kernel module之前要有kernel headers. 在virtual box虚拟机里，ubuntu已经安装了kernel headers，所以，如果要编译本机的module，直接用这个headers做编译就行。 

但是对于Raspberrypi 4.0，则需要对应kernel headers。在VM里编译，没有找到好的办法，就只能把helloworld.c放到kernel driver的源码里编译了。
- 在上一层的Makefile里加入这个路径，使make到hello目录下编译
```python
obj-y                           += hello/
```
- 在hello目录下，创建makefile


测试代码：
```python
modinfo helloworld_rpi4.ko
insmod helloworld_rpi4.ko
rmmod helloworld_rpi4.ko
lsmod
```

目前VM上的系统自带的kernel module都是经过zstd压缩过的，以'.ko.zst'为后缀，可以用unzstd来解压。 


### Error 1 while adding the time couting in the kernel module
```python
/home/junhuawa/pi_kernel/linux/drivers/char/hello/helloworld_rpi4.c:13:9: error: implicit declaration of function ‘do_gettimeofday’; did you mean ‘do_settimeofday64’? [-Werror=implicit-function-declaration]
```
#### Solution:

The  problem is that do_gettimeofday() was deprecated and is now removed from linux kernel 5.
Fixing linux time limit 2038.
It’s now replaced with ktime_get_real_ts64() or ktime_get_seconds().
With this structure:
struct timespec64 {
time64_t tv_sec; /* seconds /
long tv_nsec; / nanoseconds */
};