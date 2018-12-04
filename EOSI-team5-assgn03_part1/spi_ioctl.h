/* SPI char device IOCTL library */


#include <linux/ioctl.h>

#define IOCTL_APP_TYPE 90
#define RESET _IOR(IOCTL_APP_TYPE, 1, int)     // ioctl to RESET spi
#define CLEAR _IOR(IOCTL_APP_TYPE, 2, int)


