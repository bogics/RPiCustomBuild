/*************************************************
 * EXAMPLE kernel char device module
 * 
 * - Register char module to system 
 *   example_init: alloc_chrdev_region, cdev_init, cdev_add
 * 
 * - Passing module int and string parameters
 *   insmod example.ko int_param=5 string_param='"stevan bogic"'
 * 
 * - Unregister char module from system 
 *   example_exit: cdev_del, unregister_chrdev_region
 *   prints time in seconds elapsed since loading module
 * 
 * - Read from device
 *   example_read: copy_to_user
 * 
 * - Write to device: 
 *   example_write: 
 * 
 * - ioctl:
 *   example_ioctl - two commands implemented, to set  uppercase and lowercase of string in example_buffer
 * 
 * - Add proc interface (/proc/char_example) which gives time elapsed since loading module
 * 
 * 
 * DOCUMENTATION:
 * - http://tldp.org/LDP/lkmpg/2.6/html/index.html
 * - http://www.crashcourse.ca/introduction-linux-kernel-programming/lesson-9-all-about-module-parameters
 * - http://pointer-overloading.blogspot.rs/2013/09/linux-creating-entry-in-proc-file.html
 * - http://www.linuxdevcenter.com/pub/a/linux/2007/07/05/devhelloworld-a-simple-introduction-to-device-drivers-under-linux.html?page=1
 * - http://www.makelinux.net/ldd3/chp-3-sect-7
 * ***********************************************/
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/ctype.h> //toupper, tolower

/* Module parameter*/
static int int_param = 1;
static char *string_param = "Hello string param";
/* after compile, modinfo example.ko prints description about parameter */
MODULE_PARM_DESC(int_param, "Example of int module parameter");
MODULE_PARM_DESC(string_param, "Example of char* module parameter");
/* Third parameter: permissions of file /sys/module/example/parameters/int_param - modify on the fly */
module_param(int_param, int, 0644);
module_param(string_param, charp, 0644);

/* User-defined macros */
#define NUM_OF_DEVICES 1
#define DEVICE_NAME "char_example"


/* Global module variables*/

/* Kernel data type to represent a major / minor number pair */
static dev_t example_dev;
/* The kernel represents character drivers with a cdev structure */
static struct cdev example_cdev;


static struct timeval load_time;
static char example_buf[100];
static int example_bufsize = 100;


/**************************************************************
 * Function declarations 
 * ***********************************************************/
static int example_proc_open(struct inode *inode, struct file *file);
static int example_proc_show(struct seq_file *m, void *v); 

static ssize_t example_read(struct file *file, char __user *buf, size_t count, loff_t * ppos);
static ssize_t example_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos);
static long example_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* File operation structure */
/* Defaults for other functions (such as open, release...)
   are fine if you do not implement anything special */
static struct file_operations example_fops = {
	.owner = THIS_MODULE,
	.read = example_read,
	.write = example_write,
	/* ioctl has been renamed to unlocked_ioctl. E.g, http://www.cs.otago.ac.nz/cosc440/labs/lab06.pdf */
	.unlocked_ioctl = example_ioctl
};

/* proc file operations */
static const struct file_operations example_proc_fops = {
    .owner      = THIS_MODULE,
    .open       = example_proc_open,
    .read       = seq_read,
//    .llseek     = seq_lseek,
//    .release    = single_release,
};

/* proc dir entry */
struct proc_dir_entry *pde;
 
/**************************************************************
 * static int __init example_init(void)
 * 
 * ***********************************************************/
static int __init example_init(void)
{
	/* Dynamically register a character device major */
	if (alloc_chrdev_region(&example_dev,              /* Output: starting device number */
							0,                         /* Starting minor number, usually 0 */
							NUM_OF_DEVICES,            /* Number of device numbers */
							DEVICE_NAME) < 0) {        /* Registered name */
		printk(KERN_ERR "Cannot register device\n");
		return -1;
	}
	printk(KERN_INFO "Major number: %d\n", MAJOR(example_dev));
	printk(KERN_INFO "Minor number: %d\n", MINOR(example_dev));
	
	/* - Initialise cdev with file operations */ 
	cdev_init(&example_cdev, &example_fops);

	/* Add char module to system, with previously allocated major and minor numbers */	
	if (cdev_add(&example_cdev,       /* Character device structure */
	             example_dev,         /* Starting device major / minor number */
	             NUM_OF_DEVICES)) {   /* Number of devices */ 
		printk(KERN_ERR "Char module registration failed\n");
	}
	
	printk(KERN_INFO "Module parameter int_param: %d \n", int_param);
	printk(KERN_INFO "Module parameter string_param: %s \n", string_param);
	
	pde = proc_create("char_example", 0, NULL, &example_proc_fops);
	if (!pde) {
		printk(KERN_ERR "Cannot create proc dir char_example!\n");
		return -1;
	}
	
	/* get current time */
	do_gettimeofday(&load_time);
	printk(KERN_INFO "Char kernel module example initialized\n");
	
	strncpy(example_buf, "Initial string", example_bufsize);
	return 0;
}


/**************************************************************
 * static ssize_t example_read(struct file *file, char __user *buf, size_t count, loff_t * ppos)
 * 
 * ***********************************************************/
static ssize_t example_read(struct file *file, char __user *buf, size_t count, loff_t * ppos)
{	
	int remaining_size, transfer_size;
	
	printk(KERN_INFO "ENTER example_read\n");
	remaining_size = example_bufsize - (int)(*ppos);
	printk(KERN_INFO "example_bufsize: %d, *ppos: %llu, remaining_size: %d \n", example_bufsize, *ppos, remaining_size);
	
	/* bytes left to transfer */
	if (remaining_size == 0) {
		/* All read, returning 0 (End Of File) */
		return 0;
	}
	
	/* Size of this transfer */
	transfer_size = min_t(int, remaining_size, count);
	printk(KERN_INFO "count: %d, transfer_size: %d \n", count, transfer_size);

	if (copy_to_user(buf /* to */ , example_buf + *ppos/* from */ , transfer_size)) {
		printk(KERN_ERR "ERROR, copy_to_user return -EFAULT \n");
		return -EFAULT;
	}
	else {		
		/* Increase the position in the open file */
		*ppos += transfer_size;
		printk(KERN_INFO "OK, copy_to_user return %d \n", transfer_size);
		return transfer_size;
	}
}


/**************************************************************
 * static ssize_t example_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
 * 
 * ***********************************************************/
static ssize_t example_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{	
	/* Your code here */
	int remaining_bytes;
	
	printk(KERN_INFO "ENTER example_write\n");
	
	/* Number of bytes not written yet in the device */
	remaining_bytes = example_bufsize - (*ppos);
	printk(KERN_INFO "example_bufsize: %d, *ppos: %llu, remaining_bytes: %d \n", example_bufsize, *ppos, remaining_bytes);
	
	if (count > remaining_bytes) {
		printk(KERN_ALERT "Can't write beyond the end of the device, return -EIO\n");
		/* Can't write beyond the end of the device */
		return -EIO;
	}

	if (copy_from_user(example_buf + *ppos /*to*/ , buf /*from*/ , count)) {
		printk(KERN_ERR "ERROR, copy_from_user return -EFAULT \n");
		return -EFAULT;
	} else {
		/* Increase the position in the open file */
		*ppos += count;
		printk(KERN_INFO "OK, copy_from_user return %d \n", count);
		return count;
	}
}


/**************************************************************
 * static int example_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
 * 
 * ***********************************************************/
static long example_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int i;

	printk(KERN_INFO "ENTER example_ioctl, cmd = %d, arg = %lu\n", cmd, arg);

	switch (cmd)
	{
		case 1:
			printk(KERN_INFO "TO UPPER\n");
			i = 0;
			while (example_buf[i]) {
				example_buf[i] = toupper(example_buf[i]);
				i++;
			}
			break;

		case 2:
			printk(KERN_INFO "to lower\n");
			i = 0;
			while (example_buf[i]) {
				example_buf[i] = tolower(example_buf[i]);
				i++;
			}
			break;
/*
		case 3:
			printk("Video ram zajebancija\n");
			for (i=0; i<video_ram_bufsize; i++) {
				*((char *)video_ram_buf+i)=i;
			}
			break;		
*/			
		default:
			printk(KERN_INFO "not supported command!\n");
	}

	return retval;
}


/**************************************************************
 * static int __exit example_exit(void)
 * 
 * ***********************************************************/
static void __exit example_exit(void)
{
	struct timeval unload_time;
	
	do_gettimeofday(&unload_time);
	printk(KERN_INFO "%lu seconds have elapsed since loading module\n", unload_time.tv_sec - load_time.tv_sec);
	
	/* remove a char device from the system */
	cdev_del(&example_cdev);
	
	unregister_chrdev_region(example_dev, NUM_OF_DEVICES);
	
	remove_proc_entry("char_example", NULL);
}


/**************************************************************
 * static int example_proc_show(struct seq_file *m, void *v)
 * 
 * ***********************************************************/
static int example_proc_show(struct seq_file *m, void *v)
{
	struct timeval cur_time;
    
    do_gettimeofday(&cur_time);
    seq_printf(m, "%lu\n", cur_time.tv_sec - load_time.tv_sec);
    return 0;
}


/**************************************************************
 * static int example_proc_open(struct inode *inode, struct file *file)
 * 
 * ***********************************************************/
static int example_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, example_proc_show, NULL);
}


module_init(example_init);
module_exit(example_exit);


MODULE_AUTHOR("Stevan Bogic");
MODULE_DESCRIPTION("Char kernel module simple example");
MODULE_LICENSE("GPL");
