### 内核device和driver动态匹配过程
用raspberrypi4 B来测试platform驱动和设备的加载，match，同时测试注册GPIO中断，并能够正常工作。



### 原理
内核有device, driver, bus的概念， device由device tree定义，由内核解析device tree时生成， device tree里每个platform级的节点都定义了compatible string，driver里也定义了compatible string, 驱动加载时，会遍历platform bus上的devices做match，match成功则call driver里的probe()函数做初始化。

**注意:**
- platform bus是抽象虚拟的概念，物理上没有这个bus;
- PCI bus是物理存在的， 它的device和driver做match时是用PCI设备里读取的Vendor id和device id.

### 驱动加载过程

```python
module_platform_driver(my_platform_driver) -> platform_driver_register(drv) -> driver_register() -> bus_add_driver() -> int driver_attach(struct device_driver *drv)
--> __driver_attach(struct device *dev, void *data) --> static int driver_probe_device(struct device_driver *drv, struct device *dev)
--> int __driver_probe_device(struct device_driver *drv, struct device *dev) --> static int really_probe(struct device *dev, struct device_driver *drv)
--> static int call_driver_probe(struct device *dev, struct device_driver *drv)
--> drv->probe(dev);

driver_probe_device - attempt to bind device & driver together
```

### GPIO PIN25 中断检测
需要做2步:
- 在device tree里定义这样一个中断设备节点，将GPIO pin 25默认设成输入，下拉, 设置compatible string;
- 在driver里面设置compatible string；
- 把GPIO PIN 编号通过gpiod_to_irq()函数转换成内核可识别的虚拟中断号，即GIC中断控制器输入中断的引脚编号，调用devm_register_irq()注册中断
- 在remove函数里释放中断号 - devm_free_irq()


**注意:**
- 在dt里，中断设备节点必须在根节点下面，和soc一级，不能在gpio节点下面，或者其他节点下面，否则，platform bus找不到这个节点，match不成功！
- 要做测试时，用面包板在PIN25和VCC3.3之间连一个上拉电阻10K， 然后用GND的线触碰PIN25的线，使其电平抖动触发中断

### 遇到的问题及解决过程:
`insmod int_rpi4_key.ko` 之后没有任何输出, 有2个原因:
- 因为用了module_platform_driver()宏做初始化，也就是init函数中没有加入任何打印，所以，如果没有调用driver里的probe()，则不会有任何打印；
- 把int_key这个device tree设备节点定义在gpio这个节点里面， platform bus上找不到对应的节点


### device tree 修改
```python
diff --git a/arch/arm/boot/dts/broadcom/bcm2711-rpi-4-b.dts b/arch/arm/boot/dts/broadcom/bcm2711-rpi-4-b.dts
index d3a3a1e4d..8af754f54 100644
--- a/arch/arm/boot/dts/broadcom/bcm2711-rpi-4-b.dts
+++ b/arch/arm/boot/dts/broadcom/bcm2711-rpi-4-b.dts
@@ -44,6 +44,17 @@ sd_vcc_reg: regulator-sd-vcc {
 		enable-active-high;
 		gpio = <&expgpio 6 GPIO_ACTIVE_HIGH>;
 	};
+
+	int_key {
+		compatible = "arrow,intkey";
+
+		pinctrl-names = "default";
+		pinctrl-0 = <&key_pin>;
+		gpios = <&gpio 25 0>;
+		interrupts = <25 1>; 
+		interrupt-parent = <&gpio>;
+	};
+
 };
 
 &bt {
@@ -140,6 +151,12 @@ &gpio {
 			  "RGMII_TXD1",		/* 55 */
 			  "RGMII_TXD2",
 			  "RGMII_TXD3";
+
+	key_pin: key_pin {
+		brcm,pins = <25>;
+		brcm,function = <0>;	
+		brcm,pull = <1>; 	
+	};
 };
 
 &hdmi0 {
@@ -431,6 +448,7 @@ audio_pins: audio_pins {
 		brcm,function = <4>;
 		brcm,pull = <0>;
 	};
+
 };
 
 &led_act {

```

### driver code
```python

#include <linux/module.h>
#include <linux/platform_device.h> 	
#include <linux/interrupt.h> 		
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/of_device.h>

static char *HELLO_KEYS_NAME = "PB_KEY";

/* interrupt handler */
static irqreturn_t hello_keys_isr(int irq, void *data)
{
	//struct device *dev = data;
	printk("interrupt received. key: %s\n", HELLO_KEYS_NAME);
	return IRQ_HANDLED;
}

static struct miscdevice helloworld_miscdevice = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "mydev",
};
int irqNumber = 0;

static int my_probe(struct platform_device *pdev)
{	
	int ret_val;
	struct gpio_desc *gpio;
	struct device *dev = &pdev->dev;

	printk("my_probe() function is called.\n");
	
	/* First method to get the Linux IRQ number */
	gpio = devm_gpiod_get(dev, NULL, GPIOD_IN);
	if (IS_ERR(gpio)) {
		printk("gpio get failed\n");
		return PTR_ERR(gpio);
	}
	irqNumber = gpiod_to_irq(gpio);
	if (irqNumber < 0)
		return irqNumber;
	printk("The IRQ number is: %d\n", irqNumber);

	/* Second method to get the Linux IRQ number */
	irqNumber = platform_get_irq(pdev, 0);
	if (irqNumber < 0){
		printk( "irq is not available\n");
		return -EINVAL;
	}
	printk("IRQ_using_platform_get_irq: %d\n", irqNumber);
	
	/* Allocate the interrupt line */
	ret_val = devm_request_irq(dev, irqNumber, hello_keys_isr, 
				   IRQF_TRIGGER_FALLING,
				   HELLO_KEYS_NAME, dev);
	if (ret_val) {
		printk("Failed to request interrupt %d, error %d\n", irqNumber, ret_val);
		return ret_val;
	}
	
	ret_val = misc_register(&helloworld_miscdevice);
	if (ret_val != 0)
	{
		printk("could not register the misc device mydev\n");
		return ret_val;
	}

	printk("mydev: got minor %i\n",helloworld_miscdevice.minor);
	printk("my_probe() function is exited.\n");

	return 0;
}

static int __exit my_remove(struct platform_device *pdev)
{	
	struct device *dev = &pdev->dev;
	printk("my_remove() function is called.\n");
	devm_free_irq(dev, irqNumber, dev);
	misc_deregister(&helloworld_miscdevice);
	printk("my_remove() function is exited.\n");
	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,intkey"},
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver my_platform_driver = {
	.probe = my_probe,
	.remove = my_remove,
	.driver = {
		.name = "intkey",
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	}
};

module_platform_driver(my_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alberto Liberal <aliberal@arroweurope.com>");
MODULE_DESCRIPTION("This is a button INT platform driver");
```
### 测试结果

```python
junhuawa@raspberrypi:/$ sudo insmod int_rpi4_key.ko
[  123.007722] int_rpi4_key: loading out-of-tree module taints kernel.
[  123.014460] dtb node: nvram
[  123.017285] dtb node: soc
[  123.019901] dtb node: tr
[  123.040490] dtb node: avs-monitor
[  123.043809] dtb node: thermal
[  123.046781] dtb node: dma-controller
[  123.05036.053419] dtb node: rng
[  123.056033] dtb node: pixelvalve
[  123.059272] dtb node: pixelvalve
[  123.062507] dtb node: pixelvalve
[  123.065740] dtb node: pixelvalve
[  123.068972] dtb node: pixelvalve
[  123.072194] dtb node: clock
[  123.074991] dtb node: interrupt-controller
[  123.079091] dtb node: hdmi
[  123.081802] dtb node: i2c
[  123.084417] dtb node: hdmi
[  1de: i2c
[  123.089752] dtb node: mmcnr
[  123.092540] dtb node: firmware
[  123.095599] dtb node: clocks
[  123.098477] dtb node: gpio
[  123.101179] dtb node: reset
[  123.103963] dtb node: vcio
[  123.106664] dtb nonode: nvmem
[  123.118160] dtb node: nvmem_otp
[  123.121296] 23.141599] dtb node: timer
[  13.146997] dtb node: pcie
[  123.149699] dtb node: ethernet
[  123.152749] dtb node: dma
[  123.155359] dtb node: codec
[  123.158147] dtb node: cam1_regulator
[  123.161718] dtb node: cam_dummy_reg
[  123.165201] dtb node: leds
[  123.167898] dtb node: regulator-sd-io-1v8
[  123.171902] dtb node: regulator-sd-vcc
[  123.175645] dtb node: int_key
[  123.178694] my_probe() function is called.
[  123.182893] The IRQ number is: 54
[  123.186218] IRQ_using_platform_get_irq: 54
[  123.190450] mydev: got minor 121
[  123.193693] my_probe() function is exited.
[  123.197864] dtb node: fixedregulator_3v3
[  123.201803] dtb node: fixedregulator_5v0
[  123.205730] dtb node: v3dbus
[  123.208605] dtb node: v3d
[  123.211228] dtb node: mdio
junhuawa@raspberrypi:/$ clear
junhuawa@raspberrypi:/sys/devices/platform$ cd  int_key/
junhuawa@raspberrypi:/sys/devices/platform/int_key$ ls
driver		 modalias  power      supplier:platform:fe200000.gpio
driver_override  of_node   subsystem  uevent
junhuawa@raspberrypi:/sys/devices/platform/int_key$ ls -l
total 0
lrwxrwxrwx 1 root root    0 Jul  4 05:40 driver -> ../../../bus/platform/drivers/intkey
-rw-r--r-- 1 root root 4096 Ju1  1970 subsystem -> ../../../buey
-rw-r--r-- 1 root root 4096 /devices/platform/int_key$ cat /proc/interrupts 
           CPU0       CPU1       CPU2       CPU3       
  9:          0          0          0          0     GICv2  25 Level     vgic
 11:       3932       4704       6969       4413     GICv2  30 Level     arch_timer
 12:          0          0          0          0     GICv2  27 Level     kvm guest vtimer
 14:        299          0          0          0     GICv2  65 Level     fe00b880.mailbox
 15:          0          0          0        el     DMA IRQ
 22:          0          0          0          0     GICv2 123 Level     DMA IRQ
 27:          0          0          0          0     GICv2 175 Level     PCIe PME, aerdrv
 28:       1117          0          0          0     GICv2 189 LeveGICv2  48 Level     arm-pmu
 33    0          0     GICv2  51 L       0          0     GICv2 15vel     mmc1, mmc0
 37:       2379          0          0          0     GICv2 153 Level     uart-pl011
 38:          0        v2 106 Level     v3d
 39:          0          0          0          0     GICv2 130 Level     feb10000.codec
 40:          0          0          0          0     GICv2 129 Level     vc4 hvs
 41:          0          0          0          0  interrupt-controller@7ef00100   4 Level     vc4 hdmi hpd connected
 42:          0          0          0          0  interrupt-controller@7ef00100   5 Level     vc4 hdmi hpd disconnected
 43:          0          0          0          0  interrupt-controller@7ef00100   1 Level     vc4 hdmi cec rx
 44:          0          0          0          0  interrupt-controller@7ef00100   0 Level     vc4 hdmi cec tx
 45:          0          0          0          0  interrupt-controller@7ef00100  10 Level     vc4 hdmi hpd connected
 46:          0          0          0          0  interrupt-i hpd disconnected
 47:        0          0          0  interrupt-controller@7ef00100   8 Level     0          0          0     GICv2 107 Level     fe004000.txp
 50:          0          0          0          0     GICv2 14 Level     vc4 crtc
 51:          0          0          0        GICv2 142 Level     vc4 crtc, vc4 crtc
 52:          0          0          0          0     GICv2 133 Level     vc4 crtc
 53:          0          0          0          0     GICv2 138 Level     vc4 crtc
 54:          0          0          0          0  pinctrl-bcm2835  25 Edge      PB_KEY
IPI0:       187        221        165        170       Rescheduling interrupts
IPI1:      2887       4328       3533       4702       Function call int        0          0          0          0       CPU stop (for cwork interrupts
IPI6:         0          0          0          
Err:          0
junhuawa@raspberrypi:/sys/devices/platform/int_key$ [  380.269808] interrupt received. key: PB_KEY
[  380.274015] interrupt received. key: PB_KEY
[  380.280101] interrupt received. key: PB_KEY
[  380.289878] interrupt received. key: PB_KEY
[  380.294070] interrupt received. key: PB_KEY
[  380.311760] interrupt received. key: PB_KEY
[  380.317522] interrupt received. key: PB_KEY
[  380.321705] interrupt received. key: PB_KEY
[  380.325886] interrupt received. key: PB_KEY
[  380.330909] interrupt received. key: PB_KEY
[  386.412506] interrupt received. key: PB_KEY
[  386.416701] interrupt received. key: PB_KEY
[  386.420882] interrupt received. key: PB_KEY
[  386.425062] interrupt received. key: PB_KEY

```




