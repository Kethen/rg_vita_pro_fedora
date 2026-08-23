set -xe

script_dir=$(realpath "$0")
script_dir=$(dirname "$script_dir")

cd "$script_dir"

export rocknix_dir="../ext/rocknix_distribution"

bash prep

IMAGE_TAG="ghcr.io/rocknix/rocknix-build:latest"

podman run \
	--rm -it \
	-v "$rocknix_dir":/rocknix_dir \
	-v ./:/work_dir \
	-w /work_dir \
	--entrypoint /bin/bash \
	--userns keep-id:uid=$(id -u),gid=$(id -g) \
	"$IMAGE_TAG" \
	script
