#ifndef IOCTL_H
#define IOCTL_H

#include <linux/ioctl.h>

typedef struct {
    int i;
    int j;
} lkmc_ioctl_struct;
#define EXAMPLE_IOCTL_MAGIC 0x33
#define EXAMPLE_IOCTL_UPPER  _IOW(EXAMPLE_IOCTL_MAGIC, 0, int)
#define EXAMPLE_IOCTL_LOWER  _IOW(EXAMPLE_IOCTL_MAGIC, 1, lkmc_ioctl_struct)

#endif
