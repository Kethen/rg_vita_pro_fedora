set -xe

mkdir -p root_partition
mkdir -p boot_partition
while umount root_partition
do
	true
done
while umount boot_partition
do
	true
done

loop_dev=$(losetup -f)

losetup -P $loop_dev fedora.img

mkfs.ext4 -L FEDORA ${loop_dev}p2
mount ${loop_dev}p2 root_partition
tar -C root_partition -xf rootfs.tar

echo "LABEL=FEDORA / ext4 defaults 0 1" >> root_partition/etc/fstab

set +x
while read -r LINE
do
	new_name=$(echo "$LINE" | sed -E 's/\.xz$//')
	# there be dead symlinks in there sometimes
	if [ -h "$LINE" ]
	then
		link_target=$(readlink "$LINE")
		new_link_target=$(echo "$link_target" | sed -E 's/\.xz$//')
		ln -s "$new_link_target" "$new_name" &
	else
		xzcat "$LINE" > "$new_name" &
	fi
done <<< $(find root_partition/lib/firmware/ | grep -E '\.xz$')
wait
set -x

umount root_partition

mkfs.vfat -F 32 ${loop_dev}p1 -n ROCKNIX
mount ${loop_dev}p1 boot_partition
cp -r boot_partition_prebuilt/* boot_partition/

cp jumper_cable.img boot_partition/SYSTEM
md5sum boot_partition/SYSTEM > boot_partition/SYSTEM.md5

umount boot_partition

losetup -d ${loop_dev}
