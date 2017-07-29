#!/bin/bash

export rpi_source="$PWD"
export rpi_output="$rpi_source/output"
build_thread="3"
build_verbose="0" # 0/1
build_dryrun="0" # 0/1
	
br_defconfig="$rpi_source/config/rpi_br_defconfig"
br_skeleton="$rpi_source/config/rpi_br_skeleton"
br_target="$rpi_output/br_shadow/target/"

rpi_toolchain="$rpi_soure/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin"
nfs_dir="$rpi_output/br_shadow/images/rootfs/"

sdcard_boot=/run/media/bogic/boot/
sdcard_root=/run/media/bogic/f2100b2f-ed84-4647-b5ae-089280112716/

br_version="2015.05"

use_kernel_release="1" # 0/1
kernel_release="raspberrypi-kernel_1.20170405-1"

echo "------------------------------------------------"
echo "|               custom RPi build               |"
echo "------------------------------------------------"


rpi_build()
{
  echo "Starting Rpi build..."
  run br_make
  run kernel_build
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
kernel_build()
{
  echo "Linux kernel build"
  if [ ! -d "$rpi_output/kernel_shadow" ]; then
    shadow_create "$rpi_source/kernel" "kernel_shadow"
    run cd "$rpi_output/kernel_shadow" 
    run make bcmrpi_defconfig
  else  
    run cd "$rpi_output/kernel_shadow" 
  fi
#  run make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcmrpi_defconfig
#  run make -j $build_thread ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage modules dtbs
#  run make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=$br_target modules_install
  
  run make -j $build_thread zImage modules dtbs
  run make INSTALL_MOD_PATH=$br_target modules_install
      
  run rm -rf $rpi_output/br_shadow/images/boot
  run mkdir -p $rpi_output/br_shadow/images/boot/overlays
  run scripts/mkknlimg arch/arm/boot/zImage $rpi_output/br_shadow/images/boot/kernel.img
  run cp $rpi_output/kernel_shadow/arch/arm/boot/dts/*.dtb $rpi_output/br_shadow/images/boot/
  run cp $rpi_output/kernel_shadow/arch/arm/boot/dts/overlays/*.dtb* $rpi_output/br_shadow/images/boot/overlays/
  run cp $rpi_output/kernel_shadow/arch/arm/boot/dts/overlays/README $rpi_output/br_shadow/images/boot/overlays/
}

# kernel modules build
modules_build()
{
  echo "Kernel modules build"
  [ -d "$rpi_output/modules_shadow" ] || shadow_create "$rpi_source/modules" "modules_shadow"
  run cd $rpi_output/modules_shadow/example
  run make
  run make install
  
  echo "Example module test build"
  run cd $rpi_output/modules_shadow/example/test
  run make
  run make install
}

copy_boot_to_sdcard()
{
  #local nfs_server_ip=$(ifconfig  | grep 'inet addr:' | grep -v '127.0.0.1' | awk -F: '{print $2}' | awk '{print $1}' | head -1)
  local nfs_server_ip=$(ifconfig  | grep -w 'inet' | grep -v '127.0.0.1' | awk '{print $2}')
  if [ ! -z $1 ] && [ $1 == "nfs" ]; then
    echo "nfs mode"
    local cmdline="dwc_otg.lpm_enable=0 console=ttyAMA0,115200 ip=::::rpi::dhcp root=/dev/nfs nfsroot=$nfs_server_ip:$rpi_output/br_shadow/images/rootfs,tcp,rsize=32768,wsize=32768 elevator=deadline rootwait"
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
    run cp $rpi_output/br_shadow/images/boot/*.dtb $sdcard_boot
    
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

# gpio pin numbering: http://pi4j.com/pins/model-2b-rev1.html
# gpio example:
#	gpio mode 7 out
#	gpio write 7 1 - led on
#	gpio write 7 0 - led off
wiringPi_build()
(
  local release="f6c40cb"

  local release_info="Thu, 24 Sep 2015 23:35:31"

  echo "wiringPi build!!!"
  echo "release from: $release_info"
  if [ ! -d "$rpi_source/downloads/wiringPi-$release" ]; then
    echo "downloads/wiringPi-$release directory does not exist!"
    if [ ! -e $rpi_source/downloads/wiringPi-$release.tar.gz ]; then
      echo "ERROR $rpi_source/downloads/wiringPi-$release.tar.gz does not exist!"
      echo "Download it from https://git.drogon.net/?p=wiringPi;a=summary"
      return 1
    fi

    echo "Extracting wiringPi-$release.tar.gz ..."
    run tar -xf $rpi_source/downloads/wiringPi-$release.tar.gz -C $rpi_source/downloads
    
    # patch build.sh
    sed -i 's/$sudo make uninstall/make uninstall/g' $rpi_source/downloads/wiringPi-$release/build
    sed -i 's/$sudo make install/make install/g' $rpi_source/downloads/wiringPi-$release/build
    # patch wiringPi makefile
    sed -i 's/gcc/arm-linux-gnueabihf-gcc/g' $rpi_source/downloads/wiringPi-$release/wiringPi/Makefile
    sed -i 's|$Q ln -sf $(DESTDIR)$(PREFIX)/lib/libwiringPi.so.$(VERSION)|$Q ln -sf ./libwiringPi.so.$(VERSION)|g' $rpi_source/downloads/wiringPi-$release/wiringPi/Makefile
    # patch devLib makefile
    sed -i 's/gcc/arm-linux-gnueabihf-gcc/g' $rpi_source/downloads/wiringPi-$release/devLib/Makefile
    sed -i 's|-I.|-I. -I$(DESTDIR)$(PREFIX)/include|' $rpi_source/downloads/wiringPi-$release/devLib/Makefile
    sed -i 's|$Q ln -sf $(DESTDIR)$(PREFIX)/lib/libwiringPiDev.so.$(VERSION)|$Q ln -sf ./libwiringPiDev.so.$(VERSION)|g' $rpi_source/downloads/wiringPi-$release/devLib/Makefile
    # patch gpio makefile
    sed -i 's/gcc/arm-linux-gnueabihf-gcc/g' $rpi_source/downloads/wiringPi-$release/gpio/Makefile
    sed -i 's|	$Q chown root.root	$(DESTDIR)$(PREFIX)/bin/gpio|#	$Q chown root.root	$(DESTDIR)$(PREFIX)/bin/gpio|g' $rpi_source/downloads/wiringPi-$release/gpio/Makefile
    sed -i 's|	$Q chmod 4755		$(DESTDIR)$(PREFIX)/bin/gpio|#	$Q chmod 4755		$(DESTDIR)$(PREFIX)/bin/gpio|g' $rpi_source/downloads/wiringPi-$release/gpio/Makefile
  fi

  [ -d "$rpi_output/wiringPi_shadow" ] || shadow_create "$rpi_source/downloads/wiringPi-$release" "wiringPi_shadow"
  run cd "$rpi_output/wiringPi_shadow"
  export DESTDIR="$rpi_output/br_shadow/staging/usr"
  export PREFIX=""
  export LDCONFIG=""

  source build
  run cp -d $rpi_output/br_shadow/staging/usr/lib/libwiringPi.* $rpi_output/br_shadow/target/usr/lib/
  run cp -d $rpi_output/br_shadow/staging/usr/lib/libwiringPiDev.* $rpi_output/br_shadow/target/usr/lib/
  run cp $rpi_output/br_shadow/staging/usr/bin/gpio $rpi_output/br_shadow/target/usr/bin/
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
  if [ ! -d "$1" ]; then
    echo "Source directory doesn't exists:" "$1"
    return 0
  fi
  if [ -d "$rpi_output/$2" ]; then
    echo "Shadow directory already exists:" "$rpi_output/$2"
    return 0
  fi

  echo "Shadow create:" "$1 -> $rpi_output/$2"
  run mkdir "$rpi_output/$2"
  local tmp=$(echo "$1" | awk -F"/" '{print $NF}')
  run cd "$1/.."
  run stow -S --no-folding "$tmp" -t "$rpi_output/$2"
  run cd -> /dev/null
}

shadow_update()
{
  if [ -z $2 ]; then
    echo "Usage:" "shadow_update <src> <dst>"
    return 0
  fi
  if [ ! -d "$1" ]; then
    echo "Source directory doesn't exists:" "$1"
    return 0
  fi
  if [ ! -d "$rpi_output/$2" ]; then
    echo "Shadow directory doesn't exists:" "$rpi_output/$2"
    return 0
  fi

  echo "Shadow update:" "$1 -> $rpi_output/$2"
  local tmp=$(echo "$1" | awk -F"/" '{print $NF}')
  run cd "$1/.."
  run stow -S --no-folding -v "$tmp" -t "$rpi_output/$2"
  run cd -> /dev/null
}


## ---------------- DOWNLOAD ------------------
[ -d "$rpi_source/downloads" ] || run mkdir $rpi_source/downloads

# Buildroot
if [ ! -d "$rpi_source/buildroot" ]; then
  echo "buildroot directory does not exists!"
  if [ ! -e "$rpi_source/downloads/buildroot-$br_version.tar.bz2" ]; then
    echo "buildroot-$br_version.tar.bz2 is not found. Downloading it..."
    run wget http://buildroot.uclibc.org/downloads/buildroot-$br_version.tar.bz2 -P $rpi_source/downloads/
  fi

  if [ ! -d $rpi_source/downloads/buildroot-$br_version ]; then
    echo "Extracting buildroot-$br_version.tar.bz2..."
    run tar -xf "$rpi_source/downloads/buildroot-$br_version.tar.bz2" -C $rpi_source/downloads
  fi

  echo "Creating symlink $rpi_source/downloads/buildroot-$br_version -> $rpi_source/buildroot"
  run ln -s $rpi_source/downloads/buildroot-$br_version $rpi_source/buildroot
fi
  

### Linux kernel
if [ ! -d "$rpi_source/kernel" ]; then
  echo "linux kernle symlink does not exists!"

  if [ "$use_kernel_release" == 1 ]; then 
    echo "Linux kernel release: $kernel_release"
    
    if [ ! -e "$rpi_source/downloads/$kernel_release.tar.gz" ]; then
      echo "$kernel_release.tar.gz is not found. Downloading it..."
      run wget https://github.com/raspberrypi/linux/archive/$kernel_release.tar.gz -P $rpi_source/downloads/
    fi

    if [ ! -d $rpi_source/downloads/linux-$kernel_release ]; then 
      echo "Extracting linux-$kernel_release.tar.gz..."
      run tar -xf "$rpi_source/downloads/$kernel_release.tar.gz" -C $rpi_source/downloads
    fi
    
    echo "Creating symlink $rpi_source/downloads/linux-$kernel_release -> $rpi_source/kernel"
    run ln -s $rpi_source/downloads/linux-$kernel_release $rpi_source/kernel
  else
    echo "Latest Linux kernel from github is in use"
    if [ ! -d $rpi_source/downloads/linux ]; then
      echo "Cloning it..."
      run cd $rpi_source/downloads
      run git clone --depth=1 https://github.com/raspberrypi/linux
      run cd -
    fi

    echo "Creating symlink $rpi_source/downloads/linux -> $rpi_source/kernel"
    run ln -s $rpi_source/downloads/linux $rpi_source/kernel
  fi
fi

### Toolchain
# http://hertaville.com/2012/09/28/development-environment-raspberry-pi-cross-compiler/
if [ ! -d "$rpi_source/downloads/tools" ]; then
  run cd $rpi_source/downloads
  run git clone git://github.com/raspberrypi/tools.git
  run cd - > /dev/null
fi

if [ ! $(echo $PATH | grep $rpi_source/downloads/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin) ]; then
	export PATH=$rpi_source/downloads/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin:$PATH
fi
#if [ ! $(echo $PATH | grep $rpi_source/downloads/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/sbin) ]; then
#  export PATH=$rpi_source/downloads/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/sbin:$PATH
#fi


# check if all required packages are installed
echo "Checking if all required packages are installed..."
# check Linux distro
distro=$(uname -or)
echo "$distro"
if [[ $distro == *"ARCH"* ]]; then
  # arch
  package_list="cpio bc stow nfs-utils net-tools"
  installed_pkgs="pacman -Q"
  install_pkg="sudo pacman -S"
else
  # ubuntu
  package_list="stow build-essential nfs-kernel-server"
  installed_pkgs="dpkg -l"
  install_pkg="sudo apt-get install"
fi

missing_packages=""
for i in $package_list; do 
  $installed_pkgs | grep -w $i > /dev/null
    [ $? -eq 0 ] || missing_packages="$missing_packages $i" #echo "Package $i is missing"
  done

  if [ -z "$missing_packages" ]; then
    echo "All packages are already installed" 
  else  
    echo "missing packages: $missing_packages..."
    for i in $missing_packages; do
      echo "Installing package $i..."
      $install_pkg $i
    done
    echo "Installation finished"
  fi


# source setenv script
run source $rpi_source/setenv/rpi_setenv.sh
