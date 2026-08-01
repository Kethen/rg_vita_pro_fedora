set -xe 

script_dir=$(realpath "$0")
script_dir=$(dirname "$0")

IMAGE_TAG=rg_vita_pro_fedora_rootfs

if ! podman image exists $IMAGE_TAG
then
	podman image build -t $IMAGE_TAG -f Dockerfile --arch aarch64
fi

podman run \
	--rm -it \
	-v ../:/work_dir \
	--arch aarch64 \
	--entrypoint /bin/bash \
	localhost/$IMAGE_TAG \
	/work_dir/rootfs_build/script
