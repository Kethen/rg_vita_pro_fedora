set -xe 

script_dir=$(realpath "$0")
script_dir=$(dirname "$script_dir")

cd "$script_dir"

export rocknix_dir="../ext/rocknix_distribution"

IMAGE_TAG="rg_vita_pro_fedora_dtc_builder"

if ! podman image exists "$IMAGE_TAG"
then
	podman image build -t "$IMAGE_TAG" -f docker_file_dtc --arch $(podman info | grep OsArch | sed -E 's#\s+OsArch: linux/##')
fi

bash prep

podman run \
	--rm -it \
	-v "$rocknix_dir":/rocknix_dir \
	-v ./:/work_dir \
	-w /work_dir \
	--entrypoint /bin/bash \
	"$IMAGE_TAG" \
	script_dtc

bash cleanup
