
#include <linux/module.h>
#include <linux/platform_device.h> 	
#include <linux/interrupt.h> 		
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/of_device.h>
#include <linux/irq.h>
#include <linux/time.h>
#include <linux/delay.h>

char tasklet_data[] = "We use a string, but it could be pointer to a structure";

void my_tasklet_work(unsigned long data){
	printk("%s\n", (char *)data);
//	msleep(10); //in the tasklet, we can not sleep
}

struct tasklet_struct my_tasklet;

static char *HELLO_KEYS_NAME = "PB_KEY";

/* interrupt handler */
static irqreturn_t hello_keys_isr(int irq, void *data1)
{
	u32 trigger_type;
	trigger_type = irq_get_trigger_type((unsigned int)irq);
	printk("interrupt received. key: %s, trigger type: %d\n", HELLO_KEYS_NAME, trigger_type);
	tasklet_schedule(&my_tasklet);
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

	tasklet_init(&my_tasklet, my_tasklet_work, (unsigned long)tasklet_data);
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
				   IRQF_TRIGGER_RISING,
				   HELLO_KEYS_NAME, (void *)dev);
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
	tasklet_kill(&my_tasklet);
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


