#!/bin/bash


sudo rm -rf "$BINARIES_DIR/rootfs"
mkdir "$BINARIES_DIR/rootfs"
sudo tar xf "$BINARIES_DIR/rootfs.tar" -C "$BINARIES_DIR/rootfs"

#sudo chown 9374:9374 "$BINARIES_DIR/rootfs/home/pi"
