#!/bin/bash

set -xe

libmali_on () {
	cp -a /usr/share/libmali_etc/* /etc
	cp -a /usr/share/libmali_usr/* /usr
	ldconfig

	mkdir -p /etc/systemd/system/multi-user.target.wants

	ln -s /etc/systemd/system/fake_suspend.service /etc/systemd/system/systemd-suspend.service || true
	rm /etc/systemd/user/graphical-session.target.wants/suspend_dpms_hotfix.service || true
	ln -s /dev/null /etc/systemd/system/rtkit-daemon.service || true
	ln -s /etc/systemd/system/libmali.service /etc/systemd/system/multi-user.target.wants/libmali.service || true

	echo "libmali is now on"
}

libmali_off () {
	while read -r LINE
	do
		path="$(echo $LINE | sed 's#/usr/share/libmali_etc##')"
		path="/etc/$path"
		if [ -f "$path" ]
		then
			rm "$path"
		fi
	done <<< $(find /usr/share/libmali_etc)

	while read -r LINE
	do
		path="$(echo $LINE | sed 's#/usr/share/libmali_usr##')"
		path="/usr/$path"
		if [ -f "$path" ]
		then
			rm "$path"
		fi
	done <<< $(find /usr/share/libmali_usr)

	ldconfig

	mkdir -p /etc/systemd/user/graphical-session.target.wants

	rm /etc/systemd/system/systemd-suspend.service || true
	ln -s /etc/systemd/user/suspend_dpms_hotfix.service /etc/systemd/user/graphical-session.target.wants/suspend_dpms_hotfix.service || true
	rm /etc/systemd/system/rtkit-daemon.service || true
	rm /etc/systemd/system/multi-user.target.wants/libmali.service || true

	echo "libmali is now off"
}

if [ "$1" == "on" ]
then
	libmali_on
elif [ "$1" == "off" ]
then
	libmali_off
else
	echo "usage: sudo $0 <on/off>"
	exit 1
fi

echo "please reboot your system now to apply the change"
