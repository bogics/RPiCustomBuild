The default kernel configuration enables support for GPIO driver (built into the kernel):
CONFIG_GPIOLIB=y
CONFIG_GPIO_SYSFS=y

This built-in driver should be used for any GPIO manipulation.
Follow the link for example how to Access GPIO from Linux user space (https://falsinsoft.blogspot.rs/2012/11/access-gpio-from-linux-user-space.html)

Here presented test_gpio driver is just an example how to write kernel module. To serve the purpose, code is heavily commented.
It is not intended to be used instead of the built-in driver. 



- HARDWARE -

This GPIO driver is developed and tested on Raspberry Pi Model B+.
However, it should work on all Rapsberry Pi models. 

BCM2835 SoC is used in the Raspberry Pi Model A, B, B+, the Compute Module, and the Raspberry Pi Zero.
BCM2836 SoC is used in the Raspberry Pi 2 Model B, but the underlying architecture is identical to BCM2835.
BCM2837 SoC is used in the Raspberry Pi 3, and in later models of the Raspberry Pi 2. The underlying architecture is identical to the BCM2836.


BCM2835 has 54 general-purpose I/O (GPIO) lines. All GPIO pins have at least two alternative functions.
Note that not all GPIO pins are exposed to the external connector!

First, take a look on BCM2835-ARM-Peripherals (https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2835/BCM2835-ARM-Peripherals.pdf):
On page 5 there is the memory map diagram. There are three types of addresses:
- ARM virtual addresses
  The kernel is configured for a 1GB/3GB split between kernel and user-space memory. 
    - Virtual addresses in kernel mode will range between 0xC0000000 and 0xEFFFFFFF
    - Virtual addresses in user mode will range between 0x00000000 and 0xBFFFFFFF

- ARM physical addresses 
  The ARM section of the RAM start at 0x00000000 for RAM. 

- Bus addresses 
  The peripheral addresses specified in this document are bus addresses!
  Software directly accessing peripherals must translate these addresses into physical or virtual addresses. 

On page 90, there is the GPIO register view. The GPIO has 41 32-bit registers.
The first one is GPFSEL0 at address 0x7E200000. Note that this is the bus address!

The bus addresses for peripherals start at 0x7E000000.
According to the memory map diagram, bus address 0x7Ennnnnn is available:
- at physical address 0x20nnnnnn
- in the ARM kenel at virtual address 0xF2nnnnn





- DEVICE TREE - 

Device tree is a tree data structure with nodes that describe the physical devices in a system.
Node contains properties. 

	bcm2708_common.dtsi:
	Under "soc" property is added:
		test_gpio: test_gpio@7e200000 {
		compatible = "test_gpio";
		reg = <0x7e200000 0xb4>;
	};
	



- USAGE - 

Interaction with the test_gpio driver from userspace can be via character device or sysfs entry.

When module is loaded, /dev/test_gpio-20200000 character device is automatically created:
# insmod test_gpio.ko 
# ls -la /dev/test_gpio-20200000 
crw-------    1 root     root       10,  57 Jan  1 02:03 /dev/test_gpio-20200000

GPIO pin can be configured as input and output, with high and low values for output pin.
Pin direction, and value of the output pin, are set by writting to the device file.
To set GPIO 17 to output high:
# echo "17 high" > /dev/test_gpio-20200000
To set GPIO 17 to output low:
# echo "17 low" > /dev/test_gpio-20200000
Note that writting "high" or "low" automatically sets pin as output.

To set GPIO 26 as input:
# echo "26 in" > /dev/test_gpio-20200000

Reading from the device prints on console direction and value of all input/output pins:
# cat /dev/test_gpio-20200000

GPIO:
  0 input: 1
  1 input: 1
  2 input: 1
  ...
  ...


Module can optionally take an argument. The argument "gpio" is array of integers which represent GPIO pins for which sysfs entries will be created.
e.g.:
# insmod test_gpio.ko gpio="17,26"

# ls -la /sys/devices/platform/soc/20200000.test_gpio/testgpio*
-rw-r--r--    1 root     root          4096 Jan  1 00:05 /sys/devices/platform/soc/20200000.test_gpio/testgpio17
-rw-r--r--    1 root     root          4096 Jan  1 00:05 /sys/devices/platform/soc/20200000.test_gpio/testgpio26

To set GPIO 17 to output high:
# echo high > /sys/devices/platform/soc/20200000.test_gpio/testgpio17
To set GPIO 17 to output low:
# echo low > /sys/devices/platform/soc/20200000.test_gpio/testgpio17

To set GPIO 26 as input:
# echo in > /sys/devices/platform/soc/20200000.test_gpio/testgpio26

To read state of GPIO 26 pin:
# cat /sys/devices/platform/soc/20200000.test_gpio/testgpio26
input: 1


- IMPLEMENTATION -

It is important to understand the linux kernel driver model.
Good starting point is kernel Documentation/driver-model. Take a look at 
overview (https://www.kernel.org/doc/Documentation/driver-model/overview.txt), 
driver (https://www.kernel.org/doc/Documentation/driver-model/driver.txt),
device (https://www.kernel.org/doc/Documentation/driver-model/device.txt),
bus (https://www.kernel.org/doc/Documentation/driver-model/bus.txt),
class (https://www.kernel.org/doc/Documentation/driver-model/class.txt),
binding (https://www.kernel.org/doc/Documentation/driver-model/binding.txt),
platform (https://www.kernel.org/doc/Documentation/driver-model/platform.txt)

http://www.staroceans.org/kernel-and-driver/The%20Linux%20Kernel%20Driver%20Model.pdf


Firs step is to determine driver and device type.

This driver is implemented as platform driver.

Next, platform driver is registered with kernel.

As every loadable device driver is basically kernel module, moving platform driver into the kernel module is done with module_init() and module_exit() macros,
which registers corresponding init and exit functions. 
Init and exit functions are called when module is loaded/unloaded. These functions should call platform_driver_register() and platform_driver_unregister() macros respectively,
which take address of the previously instantiated platform_driver structure as argument.
look here (http://linuxseekernel.blogspot.rs/2014/05/platform-device-driver-practical.html) how to write simple platform driver.

This can be shortened by using the module_platform_driver() macro, which takes the previously instantiated platform_driver structure as argument.
All other is done under the hood, but as final result platform driver is registered/unregistered when module is loaded(insmod)/unloaded(rmmod).


Platform driver:

platform_device.h:
struct platform_device {
	...
	struct device	dev;
	...
};

struct platform_driver {
	...
	struct device_driver driver;
	...
};

module_platform_driver() macro will end up in the driver_register(struct device_driver *drv) function call (drivers/base/driver.c), where the 
"bus" member of the device_driver structure is set to the &platform_bus_type.


When a driver is registered, a sysfs directory is created:
/sys/bus/platform/drivers/test_gpio
This directory contains symlinks to the directories of devices it supports:
20200000.test_gpio -> ../../../../devices/platform/soc/20200000.test_gpio


ko i kada zove device_register()?


https://www.kernel.org/doc/Documentation/filesystems/sysfs.txt
https://lwn.net/Articles/69419/
