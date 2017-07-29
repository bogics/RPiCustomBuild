#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KDIR=$rpi_output/kernel_shadow
export MODULE_DEST_IMAGE=$rpi_output/br_shadow/images/rootfs/root
export MODULE_DEST_TARGET=$rpi_output/br_shadow/target/root
