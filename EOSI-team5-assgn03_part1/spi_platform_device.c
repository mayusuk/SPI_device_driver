
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
#include "spi_ioctl.h"
#include "spi_platform_device.h"

#define ZERO 0xC0
#define ONE 0xF8
#define MAX_SPI_DEV_SPEED 6666666
#define BITS_PER_WORD 8
#define CHIP_SELECT 1
#define BUS_NUM 1

#define NUMBER_OF_LED 16

//SPI pin related info

#define SpiMosiPin 11           // to be set as low for output
#define SpiLevelShifterPin 24 // to be set as low for output
#define SpiMuxPin1 44           // to be set as High
#define SpiMuxPin2 72           // to be set as Low


struct rgb {
	u8 green, blue, red;
};
struct input_data {
	int n;
	struct rgb *data;
};


//spi related code
struct spi_master *master;
static struct spi_device *spi_device;

//char device related code
int is_open = 0 ;
char message[1023];
int num_bytes ;
static dev_t SPIMajorNumber;
int count = 0;
struct device *SPIDevice;
struct class *SPIclass;

struct SPIDevice_struct{
    struct cdev cdev;
    char device_name[15];
    struct spi_device *spi;
    struct spi_transfer t;
    struct spi_message m;
    u8 *input;

}*SPIdevice_struct_pointer;

/* function to add individual LED related
colour data structure to a single big array for SPI 
transmission 
*/ 

void add_data(u8 *input, u8 color, int j){
	
	int i = 0;
	//printk("Colour** %d", color);

	for(i=0;i<8;i++,j++){
		
		if((color & 128) == 128){
			
			input[j] = ONE;		
		}else{
			input[j] = ZERO;		
		}

		color = color << 1;

	}
}

/* function to add individual LED colour data to 
data structure
*/

void setup_data(struct input_data data){
	int i = 0;
	int j = 0;
	for(i=0;i< data.n;i++){

		printk("Setting Up The data %d %d %d \n",data.data[i].green,data.data[i].blue, data.data[i].red);

		add_data(SPIdevice_struct_pointer->input, data.data[i].green, j);

		add_data(SPIdevice_struct_pointer->input, data.data[i].red, j + 8);

		add_data(SPIdevice_struct_pointer->input, data.data[i].blue, j + 16);

		j = j + 24;
	}

}


void transfer_complete(void *data){
        count++;
	printk("Transfer Completeted -- %d\n", count);
}


/* function to convert LED data array to SPI 
related structure for transfering 
*/


void spi_async_write(struct spi_device * spi,int size){
	
 	SPIdevice_struct_pointer->t.speed_hz = MAX_SPI_DEV_SPEED;
	SPIdevice_struct_pointer->t.bits_per_word = BITS_PER_WORD;
	SPIdevice_struct_pointer->t.tx_buf = SPIdevice_struct_pointer->input;
	SPIdevice_struct_pointer->t.len = size;

	spi_message_init(&SPIdevice_struct_pointer->m);
        SPIdevice_struct_pointer->m.complete = transfer_complete;
	spi_message_add_tail((void *)&SPIdevice_struct_pointer->t, &SPIdevice_struct_pointer->m);
	spi_async(spi,&SPIdevice_struct_pointer->m);

}

/* Char device Read function */
ssize_t spi_char_read (struct file * filep, char __user * outb, size_t nbytes, loff_t * offset)
{
    printk(KERN_INFO "inside read function (%s)\n",message);
    return nbytes;
}

/* char device write function to call SPI transfer function which 
initiates transfering */

ssize_t spi_char_write (struct file * filep, const char __user * inpb, size_t nbytes, loff_t * offset)
{
	struct input_data input;
	printk(KERN_INFO " inside write function (%s)\n",message);
	
	input.data = kmalloc(sizeof(struct rgb) * 16, GFP_KERNEL);
	
	if (copy_from_user(&input, inpb,nbytes)){
		return -EFAULT;
	}
	
	printk("Data Received of %d LED %d", input.n, nbytes);
	setup_data(input);
	spi_async_write(SPIdevice_struct_pointer->spi, input.n*3*8);
	return nbytes;
}



int spi_open(struct inode * inodep , struct file * filep)
{

printk(KERN_INFO "Device is openend \n");

return 0;
}



int spi_release (struct inode * inodep , struct file * filep)
{
    if(is_open ==0)
    {
        printk(KERN_INFO "error, device was never opened \n");
        return -EBUSY;
    }
    is_open =0;
    return 0;
}

/* function to setup MUX and level shifter pins for SPI
*/


void setupSPIpins(void)
{
    printk("setting up SPI pins \n");

    gpio_request(SpiMosiPin,"Spi Mosi pin");
    gpio_request(SpiLevelShifterPin,"SPI level shifter pin");
    gpio_request(SpiMuxPin1,"SPI mux1 pin");
    gpio_request(SpiMuxPin2,"SPI mux2 pin");


    gpio_direction_output(SpiMosiPin,0);
    gpio_direction_output(SpiLevelShifterPin,0);
    gpio_direction_output(SpiMuxPin1,1);
    gpio_direction_output(SpiMuxPin2,0);


    printk("SPI pins are now set \n");
}

/* IOCTL function to reset the pins as per assignment
*/

long ioctl_handle(struct file *file, unsigned int command, unsigned long ioctl_param)
{	
	int ret, i =0;
	struct input_data clear;
	
	printk(KERN_INFO "INSIDE IOCTL\n");
	switch(command){
		case RESET:
    			printk(KERN_INFO "Setting the Pins\n");
			setupSPIpins();
			SPIdevice_struct_pointer->spi->mode = SPI_MODE_0;
			SPIdevice_struct_pointer->spi->max_speed_hz = MAX_SPI_DEV_SPEED;
			SPIdevice_struct_pointer->spi->bits_per_word = BITS_PER_WORD;
			ret = spi_setup(SPIdevice_struct_pointer->spi);
			break;
		case CLEAR:
			clear.data = kmalloc(sizeof(u8)*NUMBER_OF_LED,GFP_KERNEL);
			clear.n = NUMBER_OF_LED;
			for(i=0;i< NUMBER_OF_LED;i++){
				clear.data[i].green = ZERO;
				clear.data[i].blue = ZERO;
				clear.data[i].red = ZERO;			
			}
			setup_data(clear);
			spi_async_write(SPIdevice_struct_pointer->spi, NUMBER_OF_LED*3*8);
			break;	
		default: return -EINVAL;
	}


	return 1;
}

struct file_operations fops = {
	.read = spi_char_read,
	.write = spi_char_write,
	.open = spi_open,
	.release = spi_release,
	.unlocked_ioctl = ioctl_handle,
};

/* function to free GPIO pins for required for SPI */

void freeSpiPins(void)
{
	gpio_free(SpiMosiPin);
	gpio_free(SpiLevelShifterPin);
	gpio_free(SpiMuxPin1);
	gpio_free(SpiMuxPin2);

	printk("SPI related gpio pins freed \n");

}

/* SPI probe function which will be called 
on sucessfull matching of SPI device. 
This function will register the char device required 
char device operations */


static int CHIP_probe(struct spi_device *spi)
	{

	int ret;
	printk(KERN_INFO "probe function has been started \n");

	////char device registration

	SPIclass = class_create(THIS_MODULE, Device0Name);
	SPIdevice_struct_pointer = kmalloc(sizeof(struct SPIDevice_struct), GFP_KERNEL);

   	if (alloc_chrdev_region(&SPIMajorNumber, 0, 1, Device0Name) <0)
       {
   		printk( KERN_INFO " device 0 major number not assigned");
       }
        else
       {
          	printk( KERN_INFO " device 0 major number assigned ");
       }

    	sprintf(SPIdevice_struct_pointer->device_name, Device0Name);
	cdev_init(&SPIdevice_struct_pointer->cdev, &fops);
	SPIdevice_struct_pointer->cdev.owner = THIS_MODULE;

	ret = cdev_add(&SPIdevice_struct_pointer->cdev, (SPIMajorNumber), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

        SPIDevice = device_create(SPIclass, NULL, MKDEV(MAJOR(SPIMajorNumber), 0), NULL, Device0Name);
	printk("char device created \n");

	setupSPIpins();
        SPIdevice_struct_pointer->spi = spi;
	SPIdevice_struct_pointer->input = kmalloc(sizeof(u8)*16*3*8, GFP_KERNEL);
	printk(KERN_INFO "exiting probe function \n");
	return 0;
}


static int CHIP_remove(struct spi_device *spi)
{
    printk(KERN_INFO "remove function has been called \n");
    return 1;
}


/* SPI device table required for matching 
of devices */

struct spi_device_id CHIP_id_table[] =
{
{"spidev7",0},
};

static struct spi_driver CHIP_driver = {
		.driver = {
			.name		= "spidev7",
			.owner		= THIS_MODULE
		},

		.probe		= CHIP_probe,
		.remove		= CHIP_remove,
		.id_table	= CHIP_id_table,
	};

/* init function which will be called when kernel 
module is loaded. This function will register the 
SPI driver and will creat a new SPI device.
This in turn will call the probe function
*/

static int spi_device_init(void)
{
	int vret ;
	struct spi_board_info spi_pot_device_info = {
		.modalias = "spidev7",
		.max_speed_hz = MAX_SPI_DEV_SPEED,
		.bus_num = BUS_NUM,
		.chip_select = CHIP_SELECT,
		.mode = SPI_MODE_0,
	};
	printk(KERN_INFO "inside init function \n");

	vret = spi_register_driver(&CHIP_driver);
	if(vret < 0)
	{
		printk(KERN_INFO "Driver Registraion Failed\n");
		return -1;
	}
	else printk(KERN_INFO "Driver Registraion done \n");


	//struct spi_master *master;
	master = spi_busnum_to_master( spi_pot_device_info.bus_num );
	if( !master )
		return -ENODEV;

	spi_device = spi_new_device( master, &spi_pot_device_info );
	if( !spi_device )
		return -ENODEV;

	printk(KERN_INFO "SPI device created \n");
	
	return 0;
}

/* exit function which will be called 
when kernel module is removed.
This will unregister SPI driver, device and char device
and will free SPI gpio pins
*/

static void spi_device_exit(void)
{
    printk(KERN_INFO "inside exit function \n");
    spi_unregister_driver(&CHIP_driver);
    spi_unregister_device(SPIdevice_struct_pointer->spi);
    unregister_chrdev_region(SPIMajorNumber, 1);
    device_destroy(SPIclass, MKDEV(MAJOR(SPIMajorNumber), 0));
    cdev_del(&SPIdevice_struct_pointer->cdev);
    kfree(SPIdevice_struct_pointer); 
    class_destroy(SPIclass);
    freeSpiPins();
}

module_init(spi_device_init);
module_exit(spi_device_exit);
MODULE_LICENSE("GPL");
