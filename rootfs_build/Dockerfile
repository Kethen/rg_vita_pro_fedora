FROM fedora:44

# rpmfusion
RUN dnf install -y https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm https://mirrors.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

# vita-pad2key
RUN dnf install -y python3-evdev python3-libevdev

# amixer
RUN dnf install -y alsa-utils

# extra packages
RUN dnf install -y busybox linux-firmware procps-ng iproute bash-completion fdisk nano less file

# DE
RUN dnf group install -y gnome-desktop
RUN dnf install -y 'NetworkManager*'

# install desktop apps
# can't really use flatpak until userns is figured out, aka proper kernel building instead of using prebuilt kernel
#RUN flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
#RUN flatpak install flathub -y org.mozilla.firefox org.videolan.VLC

RUN dnf install -y firefox nextcloud-client
