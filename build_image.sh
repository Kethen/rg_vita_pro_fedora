set -xe

rm -f fedora.img

BOOT_START=32768
BOOT_SIZE=$((100 * 1024 * 1024 / 512))

fallocate fedora.img -l 10G

export PATH="/usr/sbin:$PATH"

dd if=bootloader_prebuilt of=fedora.img conv=notrunc
sfdisk --delete fedora.img

printf "start=${BOOT_START}, size=${BOOT_SIZE}, bootable, type=0C\n start=$((BOOT_START + BOOT_SIZE)) type=83\n" | sfdisk fedora.img 

rm -f jumper_cable.img
(
	cd jumper_cable
	mksquashfs . ../jumper_cable.img -all-root
)

sudo bash build_image_root.sh
