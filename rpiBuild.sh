#!/bin/bash

export rpi_source="$PWD"
export rpi_output="$rpi_source/output"
build_thread="3"
build_verbose="0" # 0/1
build_dryrun="0" # 0/1
linux_tarball="1" # 0/1
	
br_defconfig="$rpi_source/config/rpi_br_defconfig"
br_skeleton="$rpi_source/config/rpi_br_skeleton"
br_target="$rpi_output/br_shadow/target/"

rpi_toolchain="$rpi_soure/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin"
nfs_dir="$rpi_output/br_shadow/images/rootfs/"

sdcard_boot=/media/bogic/boot
sdcard_root=/media/bogic/13d368bf-6dbf-4751-8ba1-88bed06bef77

br_version="2015.05"

echo "------------------------------------------------"
echo "|               custom RPi build               |"
echo "------------------------------------------------"

rpi_build()
{
  echo "Starting Rpi build..."
  run br_make
  run linux_build
}

### Buildroot
br_setenv()
{
  echo "Buildroot setenv"
  run export CCACHE_BASEDIR="$rpi_output/br_shadow"
  run export MAKEFLAGS+="BR2_JLEVEL=$build_thread V=$build_verbose"
  [ -d "$rpi_output/br_shadow" ] || run mkdir -p "$rpi_output/br_shadow"
  run cd "$rpi_output/br_shadow"
}

br_config()
{
  run br_setenv
  echo "Buildroot config"
  if [ ! -f "$rpi_output/br_shadow/.config" ]; then
    run cd "$rpi_source/buildroot"
    run make menuconfig O="$rpi_output/br_shadow"
  else
    run make menuconfig
  fi
}

br_makedefconfig()
{
  echo "Buildroot make defconfig"
  run cd "$rpi_source/buildroot"
  if [ -f "$config_dir/$br_defconfig" ]; then
    run make defconfig BR2_DEFCONFIG=$br_defconfig O="$rpi_output/br_shadow"
  else
    echo "Buildroot defconfig file not found:" "$br_defconfig"
    return 0
  fi
  
}

br_savedefconfig()
{ 
  echo "Buildroot save defconfig"
  run br_setenv
  if [ -f "$rpi_output/br_shadow/.config" ]; then
    run make savedefconfig BR2_DEFCONFIG=$br_defconfig
  else
    echo ".config file not found"
    return 0
  fi
  
}

br_make()
{
  echo "Buildroot make"

  if [ ! -d "$rpi_output/br_shadow/" ]; then
    run br_makedefconfig
  fi
  run br_setenv
  if [ -d "$rpi_output/br_shadow/target" ]; then
    run rsync -a  --exclude=".empty" $br_skeleton/ $br_target
  fi
  run make
}



# https://www.raspberrypi.org/documentation/linux/kernel/building.md
linux_build()
{
  echo "Linux kernel build"
  [ -d "$rpi_output/linux_shadow" ] || shadow_create "linux" "linux_shadow"
  run cd "$rpi_output/linux_shadow" 
  run make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcmrpi_defconfig
  run make -j $build_thread ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage modules dtbs
  run make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=$br_target modules_install
    
  run rm -rf $rpi_output/br_shadow/images/boot
  run mkdir -p $rpi_output/br_shadow/images/boot/overlays
  run scripts/mkknlimg arch/arm/boot/zImage $rpi_output/br_shadow/images/boot/kernel.img
  run cp $rpi_output/linux_shadow/arch/arm/boot/dts/*.dtb $rpi_output/br_shadow/images/boot/
  run cp arch/arm/boot/dts/overlays/*.dtb* $rpi_output/br_shadow/images/boot/overlays/
  run cp arch/arm/boot/dts/overlays/README $rpi_output/br_shadow/images/boot/overlays/
}

copy_boot_to_sdcard()
{
  local nfs_server_ip=$(ifconfig  | grep 'inet addr:' | grep -v '127.0.0.1' | awk -F: '{print $2}' | awk '{print $1}' | head -1)
  if [ ! -z $1 ] && [ $1 == "nfs" ]; then
    echo "nfs mode"
    local cmdline="dwc_otg.lpm_enable=0 console=ttyAMA0,115200 ip=::::rpi::dhcp root=/dev/nfs nfsroot=$nfs_server_ip:/home/bogic/RaspberryPi/customBuild/output/br_shadow/images/rootfs,tcp,rsize=32768,wsize=32768 elevator=deadline rootwait"
    if ! sudo exportfs | grep $nfs_dir > /dev/null; then
      sudo sh -c "echo '$nfs_dir *(rw,no_root_squash,async)' >> /etc/exports"
      sudo exportfs -a
      echo "Export folder for NFS:" "$nfs_dir"
    fi
  else
    local cmdline="dwc_otg.lpm_enable=0 console=ttyAMA0,115200 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline rootwait"
  fi  
  echo "cmdline: $cmdline"
  if [ -d "$sdcard_boot" ]; then
    run cp -f $rpi_output/br_shadow/images/boot/kernel.img $sdcard_boot 
    run rm -rf $sdcard_boot/overlays
    run cp -rf $rpi_output/br_shadow/images/boot/overlays/ $sdcard_boot
    
    run echo "$cmdline" > $sdcard_boot/cmdline.txt
  else
    echo "SD card is not mounted!!"
  fi
  
}

copy_root_to_sdcard()
{
  echo "*** copy_root_to_sdcard ***"
  echo "Erasing sdcard root partition: $sdcard_root..."
  [ ! -z $sdcard_root ] || (echo "ERROR: sdcard_root variable is not defined!!"; return 1)
  run sudo rm -rf $sdcard_root/*
  echo "Copying to sdcard root partition..."
  run sudo cp -r $rpi_output/br_shadow/images/rootfs/* $sdcard_root
}

nfs_export()
{
  if ! sudo exportfs | grep $nfs_dir > /dev/null; then
    sudo sh -c "echo '$nfs_dir *(rw,no_root_squash,async)' >> /etc/exports"
    sudo exportfs -a
    echo "Export folder for NFS:" "$nfs_dir"
  fi
}


## SW build
wiringPi_build()
(
  echo "wiringPi build"
  [ -d "$rpi_output/WiringPi_shadow" ] || shadow_create "WiringPi" "WiringPi_shadow"
  run cd "$rpi_output/WiringPi_shadow"
  source build
)

### Error detection
run()
{
  if [[ 1 -eq $build_dryrun ]]
  then
    echo "$@"
    return 0
  fi

  run_cmd="$@"

  onerr()
  {
    code=$?
    echo "Command failed ($code) :" "$run_cmd"
    read
  }

  trap onerr ERR
  "$@"
}


### Shadow build
shadow_create()
{
  if [ -z $2 ]; then
    echo "Usage:" "shadow_create <src> <dst>"
    return 0
  fi
  if [ ! -d "$rpi_source/$1" ]; then
    echo "Source directory doesn't exists:" "$rpi_source/$1"
    return 0
  fi
  if [ -d "$sdtv_output/$2" ]; then
    echo "Shadow directory already exists:" "$rpi_source/$2"
    return 0
  fi

  echo "Shadow create:" "$rpi_source/$1 -> $rpi_output/$2"
  run mkdir "$rpi_output/$2"
  run cd "$rpi_source"
  run stow -S --no-folding "$1" -t "$rpi_output/$2"
  run cd -> /dev/null
}

shadow_update()
{
  if [ -z $2 ]; then
    echo "Usage:" "shadow_update <src> <dst>"
    return 0
  fi
  if [ ! -d "$rpi_source/$1" ]; then
    echo "Source directory doesn't exists:" "$rpi_source/$1"
    return 0
  fi
  if [ ! -d "$sdtv_output/$2" ]; then
    echo "Shadow directory doesn't exists:" "$rpi_output/$2"
    return 0
  fi

  echo "Shadow update:" "$rpi_source/$1 -> $rpi_output/$2"
  run stow -S --no-folding -v "rpi_source/$1" -t "$rpi_output/$2"
}


## ---------------- DOWNLOAD ------------------
[ -d "$rpi_source/downloads" ] || run mkdir $rpi_source/downloads

# Buildroot
if [ ! -d "$rpi_source/buildroot" ]; then
  echo "buildroot directory is not present."
  if [ ! -e "$rpi_source/downloads/buildroot-$br_version.tar.bz2" ]; then
    echo "buildroot-$br_version.tar.bz2 is not found. DOwnloading it..."
    run wget http://buildroot.uclibc.org/downloads/buildroot-$br_version.tar.bz2 -P $rpi_source/downloads/
  fi

  echo "Extracting buildroot-$br_version.tar.bz2..."
  run tar -xf "$rpi_source/downloads/buildroot-$br_version.tar.bz2" -C $rpi_source/downloads
  run ln -s $rpi_source/downloads/buildroot-$br_version $rpi_source/buildroot
fi
  


### Toolchain
# http://hertaville.com/2012/09/28/development-environment-raspberry-pi-cross-compiler/
if [ ! -d "$rpi_source/tools" ]; then
  run git clone git://github.com/raspberrypi/tools.git
fi

if [ ! $(echo $PATH | grep $rpi_source/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin) ]; then
	export PATH=$rpi_source/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin:$PATH
fi
if [ ! $(echo $PATH | grep $rpi_source/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/sbin) ]; then
  export PATH=$rpi_source/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/sbin:$PATH
fi


## Extract archives
if [ "$linux_tarball" == 1 ] && [ ! -d "$rpi_source/linux-tag5" ]; then 
  echo "Extracting linux..."
  [ -e "$rpi_source/tarball/linux-tag5.tar.gz" ] || (echo "ERROR: $rpi_source/tarball/linux-tag5.tar.gz does not exists"; return 1)
  run tar -xf "$rpi_source/tarball/linux-tag5.tar.gz"
  run rm -f "$rpi_source/linux"
  run ln -s "$rpi_source/linux-tag5" "$rpi_source/linux"
fi

### Kernel
# https://www.raspberrypi.org/documentation/linux/kernel/building.md
if [ "$linux_tarball" == 0 ] && [ ! -d "$rpi_source/linux" ]; then
  git clone --depth=1 https://github.com/raspberrypi/linux
fi
