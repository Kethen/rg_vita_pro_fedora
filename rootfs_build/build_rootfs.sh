set -xe 

script_dir=$(realpath "$0")
script_dir=$(dirname "$0")
cd "$script_dir"

IMAGE_TAG=rg_vita_pro_fedora_rootfs

REBUILD_IMAGE=${REBUILD_IMAGE:-false}

if $REBUILD_IMAGE
then
	podman image rm -f $IMAGE_TAG
fi

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
