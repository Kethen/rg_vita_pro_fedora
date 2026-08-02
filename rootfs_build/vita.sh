# currently only s2idle works properly with the rocknix kernel
echo s2idle > /sys/power/mem_sleep

# audio path, oddly this isn't set correctly sometimes on UCM
CARD=rockchipes8388c
amixer -c $CARD cset name='Output 1 Playback Volume' 100%,100%
amixer -c $CARD cset name='Output 2 Playback Volume' 100%,100%
amixer -c $CARD cset name='PCM Volume' 100%,100%

# let's not waste power on a lamp, it adds up when all we got is s2idle
echo 0 > /sys/class/leds/green:power/brightness
