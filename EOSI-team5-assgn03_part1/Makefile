PWD:= $(shell pwd)
ROOT=/opt/iot-devkit/1.7.2/sysroots

KDIR:=$(ROOT)/i586-poky-linux/usr/src/kernel
SROOT:=$(ROOT)/i586-poky-linux/

CFLAGS+ = -Wall -g
CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
APP_HCSR = spi_tester

obj-m += spi_platform_device.o

all:
	make EXTRA_CFLAGS=$(CFLAGS) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -g -o $(APP_HCSR) main.c --sysroot=$(SROOT) -lpthread -Wall
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SROOT)/usr/src/kernel M=$(PWD) clean
