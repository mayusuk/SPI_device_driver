
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <asm-generic/errno.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pci.h>


#define ZERO 0xC0
#define ONE 0xF8
#define NUMBER_OF_LED 16
#define VENDOR_ID 0x8086
#define DEVICE_ID 0x0934

//pin related info

#define DirectionPin 42           // to be set as low for output
#define GpioPin12 15           // Actual GPIO pin to send data


#define DeviceName "BitBangDev"

spinlock_t mLock;
unsigned long flags;


struct rgb {
	u8 green, blue, red;
};
struct input_data {
	int n;
	struct rgb *data;
};


//char device related code

static dev_t MajorNumber;  // Major number for character device
int count = 0;
struct device *ChrDevice;
struct class *Class;


/**
	Character Device Structure
**/

struct ChrDevice_struct{
    struct cdev cdev;
    char device_name[15];
    u8 *input;
    int input_size;
}*ChrDevice_struct_pointer;



/**
	This code find the base address of the gpio registers.
	Using That It finds the data registers offset for the GPIO.
	Using the ktime_get_ns to get the nano seconds from the boot time,
	we are able to get the most closer waveform for LED ring to drive
	Using the Spin lock to avoid preemption.
**/

int bit_bang(void){
	void __iomem *reg_base;
	void __iomem *reg_data;
	int size;
	u32 val_data = 0;
	int i;
	u64 t;
	resource_size_t start = 0, len = 0;

	// get pci_dev from vendor id and device id
	struct pci_dev *gpio_pci_dev = pci_get_device(VENDOR_ID,DEVICE_ID, NULL);
	start = pci_resource_start(gpio_pci_dev, 1);
	len = pci_resource_len(gpio_pci_dev, 1);
	if (!start || len == 0) {
		printk("ERROR LEN\n");
		dev_err(&gpio_pci_dev->dev, "bar%d not set\n", 1);
		goto exit;
	}
	
	// map to the memory region of the 
	reg_base = ioremap_nocache(start, len);
	if (NULL == reg_base) {
		printk("ERROR REG BASE\n");
		dev_err(&gpio_pci_dev->dev, "I/O memory remapping failed\n");
		goto exit;
	}
	
	reg_data = reg_base + 0x00;

	t = ktime_get_ns();
	spin_lock_irqsave(&mLock, flags);   // to avoid preemption
	val_data = ioread32(reg_data);		// Reading the data from the GPIO data registers and then chnaging the required bit only
	size = ChrDevice_struct_pointer->input_size*3*8;


	// Following code does the bitbanging on the gpio port. Timings are adjusted to best achieve the closests waveform.

	for(i=0;i<size;i++){
		if(ChrDevice_struct_pointer->input[i] == ONE){	
			// If need to send the data bit 1, Sending the corresponding waveform		
			iowrite32(val_data | BIT(7 % 32), reg_data);
			t = t + 700;
			while (ktime_get_ns() < t);		
			iowrite32(val_data & ~BIT(7 % 32), reg_data);
			t = t + 500;
			while (ktime_get_ns() < t);
		}else{
			// If need to send the data bit 0, Sending the corresponding waveform	
			iowrite32(val_data | BIT(7 % 32), reg_data);	
			t = t + 300;
			while (ktime_get_ns() < t);			
			iowrite32(val_data & ~BIT(7 % 32), reg_data);
			t = t + 700;
			while (ktime_get_ns() < t);
		}
	} 
	spin_unlock_irqrestore(&mLock, flags);
	
	goto success;

	exit:
		return -1;
	success:
		return 1;

}
void add_data(u8 *input, u8 color, int j){
	
	int i = 0;
	for(i=0;i<8;i++,j++){
		
		if((color & 128) == 128){
			input[j] = ONE;		
		}else{
			input[j] = ZERO;		
		}

		color = color << 1;

	}
}

void setup_data(struct input_data data){
	int i = 0;
	int j = 0;
	ChrDevice_struct_pointer->input_size = data.n;
	for(i=0;i< data.n;i++){

		printk("Setting Up The data %d %d %d \n",data.data[i].green,data.data[i].blue, data.data[i].red);

		add_data(ChrDevice_struct_pointer->input, data.data[i].green, j);

		add_data(ChrDevice_struct_pointer->input, data.data[i].red, j + 8);

		add_data(ChrDevice_struct_pointer->input, data.data[i].blue, j + 16);

		j = j + 24;
	}	

}

ssize_t char_read (struct file * filep, char __user * outb, size_t nbytes, loff_t * offset)
{
	    printk(KERN_INFO "inside read function\n");
	    return nbytes;
}


/**
	Reading the input for the number of LED from the userspace and starting the bit banging
	struct input_data : this is the data represensation for RGB LED ring
**/
ssize_t char_write (struct file * filep, const char __user * inpb, size_t nbytes, loff_t * offset)
{
	struct input_data input;
	printk(KERN_INFO " inside write function \n");
	input.data = kmalloc(sizeof(struct rgb) * 16, GFP_KERNEL);
	
	if (copy_from_user(&input, inpb,nbytes)){
		return -EFAULT;
	}
	
	printk("Data Received of %d LED %d", input.n, nbytes);
	
	setup_data(input);
	bit_bang();	
	
	return nbytes;
}


int open(struct inode * inodep , struct file * filep)
{

	printk(KERN_INFO "Device is openend \n");

	return 0;
}

int release (struct inode * inodep , struct file * filep)
{
    return 0;
}


/**
	Setting up the GPIO pin
	IO12 is getting setup for output
	DIrection pin - 42
	GPIOpin - 15
	
**/

void setupSPIpins(void)
{
    printk("setting up SPI pins \n");

    gpio_request(DirectionPin,"direction pin for 12");
    gpio_request(GpioPin12,"data pin for gpio 12");

    gpio_direction_output(DirectionPin,0);
    gpio_direction_output(GpioPin12,0);

    printk("SPI pins are now set \n");
}


/**
	File operations for the character device
**/

struct file_operations fops = {
	.read = char_read,
	.write = char_write,
	.open = open,
	.release = release,
};



/**
	Free the gpio pin on removing the character device
**/
void freeSpiPins(void)
{
	gpio_free(DirectionPin);
	gpio_free(GpioPin12);

	printk("gpio pins freed \n");

}



/**
	Initilaisation of the chracter device
**/
static int device_init(void)
{
	int ret;
	printk(KERN_INFO "probe function has been started \n");

	////char device registration

	Class = class_create(THIS_MODULE, DeviceName);
	ChrDevice_struct_pointer = kmalloc(sizeof(struct ChrDevice_struct), GFP_KERNEL);

   	if (alloc_chrdev_region(&MajorNumber, 0, 1, DeviceName) <0)
       {
   		printk( KERN_INFO " device 0 major number not assigned");
       }
        else
       {
          	printk( KERN_INFO " device 0 major number assigned ");
       }

    	sprintf(ChrDevice_struct_pointer->device_name, DeviceName);
	cdev_init(&ChrDevice_struct_pointer->cdev, &fops);
	ChrDevice_struct_pointer->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ChrDevice_struct_pointer->cdev, (MajorNumber), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

        ChrDevice = device_create(Class, NULL, MKDEV(MAJOR(MajorNumber), 0), NULL, DeviceName);
	printk("char device created \n");


	// setup the gpo pins
	setupSPIpins();

	// allocate memory for the input structure of LED ring data
	ChrDevice_struct_pointer->input = kmalloc(sizeof(u8)*NUMBER_OF_LED*3*8, GFP_KERNEL);

	printk(KERN_INFO "exiting probe function \n");
	return 0;
}


static void device_exit(void)
{
	    printk(KERN_INFO "inside exit function \n");
	    unregister_chrdev_region(MajorNumber, 1);    // unregister charddev region
	    device_destroy(Class, MKDEV(MAJOR(MajorNumber), 0));  // destory the character device
	    cdev_del(&ChrDevice_struct_pointer->cdev);
	    kfree(ChrDevice_struct_pointer);  // free character device struct 
	    class_destroy(Class);
	    freeSpiPins();   // free gpio pins
}

module_init(device_init);
module_exit(device_exit);
MODULE_LICENSE("GPL");
