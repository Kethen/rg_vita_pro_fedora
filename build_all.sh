set -xe
bash kernel_build/build_kernel_and_dtc.sh
REBUILD_IMAGE=true bash rootfs_build/build_rootfs.sh
bash build_image.sh
