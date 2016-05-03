MODULE_NAME = example

PWD := $(shell pwd)

obj-m := $(MODULE_NAME).o

all:
	make -C $(KDIR) M=$(PWD) modules

install:
	cp $(MODULE_NAME).ko $(MODULE_DEST_TARGET)
	sudo cp $(MODULE_NAME).ko $(MODULE_DEST_IMAGE)

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f $(MODULE_DEST_TARGET)/$(MODULE_NAME).ko
	sudo rm -f $(MODULE_DEST_IMAGE)/$(MODULE_NAME).ko
