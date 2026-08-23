set -xe

script_dir=$(realpath "$0")
script_dir=$(dirname "$script_dir")

cd "$script_dir"

bash build_dtc_podman.sh
bash build_podman.sh
