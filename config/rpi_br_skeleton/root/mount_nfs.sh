#/bin/sh
[ -d /mnt/nfs ] || mkdir /mnt/nfs

mount | grep "/mnt/nfs" > /dev/null
res=$?

if [ $res -ne 0 ]; then
  mount -t nfs -o nolock 192.168.0.11:/home/bogic/work/rpi/new_build/RPiCustomBuild/output/br_shadow /mnt/nfs/
fi
