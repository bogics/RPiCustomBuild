// http://xillybus.com/tutorials/device-tree-zynq-4

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#define BCM2835_NUM_GPIOS 54

/* Module parameters */
static int gpio[BCM2835_NUM_GPIOS];
static int gpio_argc = 0;
module_param_array(gpio, int, &gpio_argc, 0644);

// Taken from pinctrl-bcm2835.c !!!!
/* GPIO register offsets */
#define GPFSEL0		0x0	/* Function Select */
#define GPFSEL1		0x4
#define GPSET0		0x1c	/* Pin Output Set */
#define GPCLR0		0x28	/* Pin Output Clear */
#define GPLEV0		0x34	/* Pin Level */
#define GPEDS0		0x40	/* Pin Event Detect Status */
#define GPREN0		0x4c	/* Pin Rising Edge Detect Enable */
#define GPFEN0		0x58	/* Pin Falling Edge Detect Enable */
#define GPHEN0		0x64	/* Pin High Detect Enable */
#define GPLEN0		0x70	/* Pin Low Detect Enable */
#define GPAREN0		0x7c	/* Pin Async Rising Edge Detect */
#define GPAFEN0		0x88	/* Pin Async Falling Edge Detect */
#define GPPUD		0x94	/* Pin Pull-up/down Enable */
#define GPPUDCLK0	0x98	/* Pin Pull-up/down Enable Clock */

#define FSEL_REG(p)		(GPFSEL0 + (((p) / 10) * 4))
#define FSEL_SHIFT(p)		(((p) % 10) * 3)
#define GPIO_REG_OFFSET(p)	((p) / 32)
#define GPIO_REG_SHIFT(p)	((p) % 32)

enum output_level {
	OUTPUT_LOW,
	OUTPUT_HIGH,
	OUTPUT_MAX
};
struct test_gpio_dev {
	/* miscdev struct is used to handle multiple devices */
	struct miscdevice miscdev;
	void __iomem *regs;
	struct device_attribute **dev_attr;
	char **sysfiles;
};

static ssize_t test_gpio_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos);
static ssize_t test_gpio_read(struct file *file, char __user *buf, size_t count, loff_t * ppos);

static const struct file_operations test_gpio_fops = {
    .owner      = THIS_MODULE,
    .write      = test_gpio_write,
	.read       = test_gpio_read
};

static unsigned int reg_read(struct test_gpio_dev *dev, int off)
{
	return readl(dev->regs + off);
}

static void reg_write(struct test_gpio_dev *dev, int val, int off)
{
	writel(val, dev->regs + off);
	return;
}

static int set_output(struct test_gpio_dev *dev, char pin, enum output_level out) {
	int offset, val;

	/* RED LED is connected to GPIO17, e.g. to turn it on: */
	// GPFSEL1, bits 23-21 -> 001 = GPIO Pin 17 is an output
	// GPSET0, set pin 17
	/* GREEN LED is connected to GPIO26 */

	switch (out) {
	case OUTPUT_LOW:
		offset = GPCLR0 + (pin / 32) * 4;
		break;

	case OUTPUT_HIGH:
		offset = GPSET0 + (pin / 32) * 4;
		break;

	default:
		printk(KERN_ALERT "\n[%s][%d] ERROR: Invalid argument!\n", __FUNCTION__, __LINE__);
	}

	val = 0x1;
	val <<= FSEL_SHIFT(pin);
	reg_write(dev, reg_read(dev, FSEL_REG(pin)) | val, FSEL_REG(pin));
	val = 0x1;
	val <<= pin;
	reg_write(dev, val, offset);

	return 0;
}

static int set_input(struct test_gpio_dev *dev, char pin) {
	int val;

	/* e.g, Switch is connected to GPIO17, e.g. to set it as input: */
	// GPFSEL1, bits 23-21 -> 000 = GPIO Pin 17 is an output

	val = 0x3;
	val <<= FSEL_SHIFT(pin);
	reg_write(dev, reg_read(dev, FSEL_REG(pin)) & ~val, FSEL_REG(pin));

	return 0;
}

static ssize_t test_gpio_read(struct file *file, char __user *buf, size_t count, loff_t * ppos)
{
	struct test_gpio_dev *dev = container_of(file->private_data, struct test_gpio_dev, miscdev);
	static int pin = -1;
	int i, val, level;
	char tmp_buf[200] = {0};
	int transfer_size;

//	printk(KERN_ALERT "\n----- [%s] [%d] ENTER!!!!, *ppos: %llu\n", __FUNCTION__, __LINE__, *ppos);

	if (pin == -1) {
		snprintf(tmp_buf, sizeof(tmp_buf), "\nGPIO:");
		pin++;
		goto transfer;
	}
	for (i = pin ; i < BCM2835_NUM_GPIOS; i++) {
		val = reg_read(dev, FSEL_REG(pin));
//		printk(KERN_ALERT "\n    - val: 0x%x", val);
		val = (val >> (pin%10)*3) & 7;
//		printk(KERN_ALERT "\n    - val_final: 0x%x", val);

		if (val == 0 || val == 1) {
			level = reg_read(dev, GPLEV0 + (pin / 32) * 4);
			level = (level >> pin) & 1;
			if (val == 0) {
//				printk(KERN_ALERT "\n Pin %d: input", pin);
				snprintf(tmp_buf, sizeof(tmp_buf), "\n  %d input: %d", i, level);
			}
			else
//				printk(KERN_ALERT "\n Pin %d: output", pin);
				snprintf(tmp_buf, sizeof(tmp_buf), "\n  %d output: %d", i, level);
		}
			pin++;
			goto transfer;
	}

	pin = -1;
	return 0;

transfer:
	transfer_size = strlen(tmp_buf) + 1;

	//		printk(KERN_ALERT "\n----- [%s] [%d] tmp_buf: %s \n", __FUNCTION__, __LINE__, tmp_buf);
	if (copy_to_user(buf /* to */ , tmp_buf + *ppos/* from */ , transfer_size)) {
		printk(KERN_ERR "ERROR, copy_to_user return -EFAULT \n");
		return -EFAULT;
	}
	return (transfer_size);
}


static ssize_t test_gpio_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	/* The ﬁrst thing to do is to retrieve the test_gpio_dev structure from the miscdevice structure itself,
	 * accessible through the private_data ﬁeld of the open ﬁle structure (file).
	 * At the time we registered our misc device, we didn’t keep any pointer to the test_gpio_dev structure.
	 * However, as the miscdevice structure is accessible through file->private_data, and is a member of the test_gpio_dev structure,
	 * we can use a magic macro to compute the address of the parent structure:
	 *
	 * see: http://radek.io/2012/11/10/magical-container_of-macro/
	 */
	struct test_gpio_dev *dev = container_of(file->private_data, struct test_gpio_dev, miscdev);
	int pass=0;
	char *input;
	const char *tmp;
	char cmd[20], pin;
	int err;

	input = kzalloc(count, GFP_ATOMIC);
	if (input == NULL) {
		err = -ENOMEM;
		goto out_err;
	}

	/* Last character is 0xA (NL line feed). kzalloc initialize allicated memory to zeros.
	 * Copy count-1 bytes only (without 0xA), last allocated byte in input is NULL terminator */
	if (copy_from_user(input, buf, count-1)) {
		err = -EFAULT;
		goto out_err;
	}

	while ((tmp = strsep((char **)&input, " ")) != NULL) {
		if (!*tmp) {
			break;
		}

		switch (pass++) {
		case 0:
			pin = (char) simple_strtol(tmp, NULL, 0);
			printk(KERN_ALERT "\npin: %d\n", pin);
			break;
		case 1:
			strncpy(cmd, tmp, sizeof(cmd));
			printk(KERN_ALERT "\ncmd: %s, strlen(tmp): %d\n", cmd, strlen(tmp));
			break;
		default:
			break;
		}
	}

	kfree(input);


	if (strcmp(cmd, "high") == 0) {
		set_output(dev, pin, OUTPUT_HIGH);
	}
	else if (strcmp(cmd, "low") == 0) {
		set_output(dev, pin, OUTPUT_LOW);
	}
	else if (strcmp(cmd, "in") == 0) {
		set_input(dev, pin);
	}
	else {
		printk(KERN_ALERT "\nERROR: Invalid command!\n");
		err = count;
		goto out_err;
	}


	printk(KERN_ALERT "\n----- [%s] [%d] OK!!!\n", __FUNCTION__, __LINE__);
	return count;

out_err:
	return err;
}

static ssize_t test_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *output = "test_gpio_show";
	printk(KERN_ALERT "\n----- [%s] [%d] ENTER!!!\n", __FUNCTION__, __LINE__);
	return sprintf(buf, "%s\n", output);
}

static ssize_t test_gpio_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *tmp, pin;
	char *fname = kzalloc(strlen(attr->attr.name) + 1, GFP_KERNEL);
	struct test_gpio_dev *mydrv = dev_get_drvdata(dev);

	strcpy(fname, attr->attr.name);

	printk(KERN_ALERT "\n----- [%s] [%d] ENTER!!! count: %d, buf: %s, name: %s\n", __FUNCTION__, __LINE__, count, buf, attr->attr.name);
	/* attr->attr.name is testgpioX, get pin number X from it: */
	tmp = strsep((char **)&fname, "o");
	pin = (char) simple_strtol(fname, NULL, 0);
	kfree(fname);
	printk(KERN_ALERT "\n----- [%s] [%d] ENTER!!! pin: %d\n", __FUNCTION__, __LINE__, pin);

	if (strncmp(buf, "high", 4) == 0) {
		set_output(mydrv, pin, OUTPUT_HIGH);
	}
	else if (strncmp(buf, "low", 3) == 0) {
		set_output(mydrv, pin, OUTPUT_LOW);
	}
	else {
		printk(KERN_ALERT "\nERROR: Invalid command!\n");
//		err = count;
//		goto out_err;
	}
	return count;
}


#ifdef CONFIG_OF
static struct of_device_id test_gpio_dt_match[] = {
	{ .compatible = "test_gpio", },
	{ },
};
MODULE_DEVICE_TABLE(of, test_gpio_dt_match);
#endif

static int test_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct resource *regs;
	struct test_gpio_dev *dev;
//	int val;
	int err, i;
	char name[20];

	//pr_info("Called test_gpio, id->name: %s\n", id->name);
	pr_info("Called test_gpio_probe\n");

	/* The first operation is a sanity check, verifying that the probe was called on a device that is relevant.
	 * This is probably not really necessary, but this check appears in many drivers. */
	match = of_match_device(test_gpio_dt_match, &pdev->dev);
	if (!match)
		return -EINVAL;

	/* Get the device memory range from the device tree */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "could not get IO memory\n");
		return -ENXIO;
	}
	pr_info("\nphysical address: 0x%x\n", regs->start); //physical address: 0x20200000
	if (regs->name)
		pr_info("\n~~~~~ regs->name: %s\n", regs->name); //~~~~~ regs->name: /soc/test_gpio@7e215000

	dev = devm_kzalloc(&pdev->dev, sizeof(struct test_gpio_dev), GFP_KERNEL);



	/* map the device physical memory into the virtual address space  */

/* !!! Cannot do it with devm_ioremap_resource because it internaly calls request_mem_region.
 *     As devm_ioremap_resource is already called from pinctrl-bcm2835, another call of
 *     request_mem_region on same memory region will cause an error.
 **/
#if 0
	dev->regs = devm_ioremap_resource(&pdev->dev, regs);
	if (!dev->regs) {
		dev_err(&pdev->dev, "Cannot remap registers\n");
		return -ENOMEM;
	}
#endif
/* request_mem_region tells the kernel that driver is going to use this range of I/O addresses,
 * which will prevent other drivers to make any overlapping call to the same region through request_mem_region.
 * This mechanism does not do any kind of mapping, it's a pure reservation mechanism,
 * which relies on the fact that all kernel device drivers must be nice, and they must call request_mem_region,
 * check the return value, and behave properly in case of error.
 */
#if 0
	pr_info("\n++ request_mem_region\n");
	if(request_mem_region(regs->start, resource_size(regs), regs->name) == NULL )
	{
		dev_err(&pdev->dev, "unable to obtain I/O memory address 0x%x\n", regs->start);
		return -EBUSY;
	}
#endif


	/* Instead of the devm_ioremap_resource, go with low level ioremap, without requsting memory region... */

/* NOTICE:
 *   Using request_mem_region() and ioremap() in device drivers is now deprecated.
 *   You should use the below "managed" functions instead, which simplify driver coding and error handling:
 *   	▶ devm_ioremap()
 *   	▶ devm_iounmap()
 *   	▶ devm_ioremap_resource()
 *   		▶ Takes care of both the request and remapping operations!
 */
#if 0
	dev->regs  = ioremap(regs->start, resource_size(regs));
	if (!dev->regs) {
		dev_err(&pdev->dev, "could not remap memory\n");
		return -1;
	}
#endif
	dev->regs = devm_ioremap(&pdev->dev, regs->start, resource_size(regs));
	if (dev->regs == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() registers\n");
		return -ENODEV;
	}
	pr_info("\nvirtual address: 0x%x!!!\n", (int)dev->regs); //virtual address: 0xf2200000


	/* IMPLEMENTATION OF CHARACTER DRIVER USING MISC FRAMEWORK
	 *
	 * MISC subsistem is a thin layer above the characted driver API. It is intended for devices that really do not fit in
	 * any of the existing frameworks (input, network, video, audio, etc).
	 * Another advantage is that devices are integrated in the Device Model (device ﬁles appearing in devtmpfs,
	 * which you don't have with raw character devices).
	 */
	//static DEVICE_ATTR(testgpio, S_IWUSR | S_IRUGO, test_gpio_show, test_gpio_store);
	if (gpio_argc > 0) {
		dev->dev_attr = devm_kzalloc(&pdev->dev, gpio_argc * sizeof(struct device_attribute), GFP_ATOMIC);
		dev->sysfiles = devm_kzalloc(&pdev->dev, gpio_argc * sizeof(dev->sysfiles), GFP_ATOMIC);
		for (i = 0; i < gpio_argc; i++) {
			pr_info("\n Param %d. %d\n", i, gpio[i]);
			snprintf(name, sizeof(name), "testgpio%d", gpio[i]);
			pr_info("\n name: %s\n", name);
	//		dev->dev_attr[i] = __ATTR(testgpio, S_IWUSR | S_IRUGO, test_gpio_show, test_gpio_store);
			dev->sysfiles[i] = devm_kzalloc(&pdev->dev, strlen(name)+1, GFP_ATOMIC);
			strcpy(dev->sysfiles[i], name);

			dev->dev_attr[i] = devm_kzalloc(&pdev->dev, sizeof(struct device_attribute), GFP_ATOMIC);
			dev->dev_attr[i]->attr.name = dev->sysfiles[i];
			dev->dev_attr[i]->attr.mode = VERIFY_OCTAL_PERMISSIONS(S_IWUSR | S_IRUGO);
			dev->dev_attr[i]->show	= test_gpio_show;
			dev->dev_attr[i]->store	= test_gpio_store;
			//		device_create_file(&pdev->dev, &dev_attr_testgpio);
			device_create_file(&pdev->dev, dev->dev_attr[i]);
		}
	}
	/* In order to deal with usual constraint of handling multiple devices, miscdev struct is added to our driver specifc private data structure.
	 * To be able to access our private data structure in other parts of the driver, dev struct is attached to the pdev structure using the
	 * platform_set_drvdata() function.
	 */
	platform_set_drvdata(pdev, dev);

	/* At the end of the probe() routine, when the device is fully ready to work, the miscdevice structure is initialized for each found device:
	 * • To get an automatically assigned minor number.
	 * • To specify a name for the device ﬁle in devtmpfs. We propose to use devm_kasprintf(&pdev->dev, GFP_KERNEL, "test_gpio-%x", res->start).
	 *   devm_kasprintf() allocates a buﬀer and runs ksprintf() to ﬁll its contents.
	 * • To pass the ﬁle operations structure prevously defined.
	 */
	dev->miscdev.fops = &test_gpio_fops;
	dev->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "test_gpio-%x", regs->start);
	dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	err = misc_register(&dev->miscdev);
	if (err < 0)
		return err;

//	device_create_file(&pdev->dev, &dev_attr_testgpio);

	/* SUMMARY:
 * In device tree (bcm2708_common.dtsi), "test_gpio" node is defined as child node of the "soc".
 * "test_gpio" has reg property (refers to a range of units in a register space):
 * 	reg = <0x7e200000 0xb4>;
 * Notice that information about number of cells is held in following declarations of ancestor node (soc):
 * 	#address-cells = <1>;
	#size-cells = <1>;
 * That means, 0x7e200000 is start address and 0xb4 is size (length) of the range.
 * 0x7e200000 is BUS ADDRESS, ref to Broadcom-BC2835-datasheet.pdf, pages 5 and 6.
 *
 * From driver, this range is retrieved using platform_get_resource function with IORESOURCE_MEM parameter, but
 * PHYSICAL 0x20200000 address is returned!!
 *
 * Physical address needs to be translated to VIRTUAL ADDRESS IN KERNEL MODE 0xf2200000 using devm_ioremap()
 */

	pr_info("test_gpio_probe OK!!!!!\n");
	return 0;
}

static int test_gpio_remove(struct platform_device *pdev)
{
	int i;
	struct test_gpio_dev *dev = platform_get_drvdata(pdev);

	pr_info("Called test_gpio_remove\n");
//	device_remove_file(&pdev->dev, &dev_attr_testgpio);
	for (i = 0; i < gpio_argc; i++) {
		pr_info("device_remove_file: %s\n", dev->dev_attr[i]->attr.name);
		device_remove_file(&pdev->dev, dev->dev_attr[i]);
	}

	misc_deregister(&dev->miscdev);


	pr_info("test_gpio_remove OK!!!!!\n");
	
	return 0;
}


static struct platform_driver test_gpio_driver = {
	.driver = {
		.name = "test_gpio",
		.owner = THIS_MODULE,
		.of_match_table = test_gpio_dt_match,
	},
	.probe = test_gpio_probe,
	.remove = test_gpio_remove,
};

module_platform_driver(test_gpio_driver);

MODULE_AUTHOR("Stevan Bogic <bogics@gmail.com>");
MODULE_DESCRIPTION("Raspberry Pi GPIO kernel module");
MODULE_LICENSE("GPL"); // Get rid of taint message by declaring code as GPL.
