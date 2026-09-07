set -xe

if [ -e /.expanded ]
then
	exit 0
fi

BOOT_START=32768
BOOT_SIZE=$((100 * 1024 * 1024 / 512))

root_device=$(df / | grep /dev | awk '{print $1}' | sed -E 's#(/dev/mmcblk[0-9]+)p*[0-9]+#\1#')

if [ -z "$root_device" ]
then
	echo root device not found
fi

printf "start=${BOOT_START}, size=${BOOT_SIZE}, bootable, type=0C\n start=$((BOOT_START + BOOT_SIZE)) type=83\n" | sfdisk --force $root_device
partprobe $root_device
btrfs filesystem resize max /

touch /.expanded
