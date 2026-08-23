# currently only s2idle works properly with the rocknix kernel
echo s2idle > /sys/power/mem_sleep

# fix uid/gid mapping
setcap cap_setuid=ep /usr/bin/newuidmap
setcap cap_setgid=ep /usr/bin/newgidmap

# audio path, oddly this isn't set correctly sometimes on UCM
CARD=rockchipes8388c
amixer -c $CARD cset name='Output 1 Playback Volume' 70%,70%
amixer -c $CARD cset name='Output 2 Playback Volume' 70%,70%
amixer -c $CARD cset name='PCM Volume' 100%,100%

# let's not waste power on a lamp, it adds up when all we got is s2idle
echo 0 > /sys/class/leds/green:power/brightness

# make sure / is mounted as shared, jump cable quirk
mount -o remount,shared /
