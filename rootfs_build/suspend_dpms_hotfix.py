from gi.repository import GLib
import dbus
from dbus.mainloop.glib import DBusGMainLoop
import subprocess

DBusGMainLoop(set_as_default=True)

session_bus = dbus.SessionBus()
system_bus = dbus.SystemBus()
dbus_object = session_bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus")
dbus_object.BecomeMonitor(["path='/org/gnome/Mutter/DisplayConfig'"], dbus.UInt32(0))
#dbus_system_object = system_bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus")
#dbus_system_object.BecomeMonitor(["path='/org/freedesktop/systemd1'"], dbus.UInt32(0))

# this does not work, odd
#session_bus.get_object("org.gnome.Mutter.DisplayConfig", "/org/gnome/Mutter/DisplayConfig", introspect=False)

def display_back_on():
	subprocess.run("busctl --user set-property org.gnome.Mutter.DisplayConfig /org/gnome/Mutter/DisplayConfig org.gnome.Mutter.DisplayConfig PowerSaveMode i 0", shell=True)

display_off = False
preparing_to_sleep = False

def on_display_object_message(bus, message):
	global display_off
	if message.get_path() != "/org/gnome/Mutter/DisplayConfig":
		return
	type = message.get_type()
	if type != 4:
		return
	sender = message.get_sender()
	change = message.get_args_list()[1]
	if "PowerSaveMode" not in change:
		return
	mode = change["PowerSaveMode"]

	if mode == 3:
		display_off = True
		print("display off")
		if preparing_to_sleep:
			print("turning diplay back on as we are going into suspend")
			display_back_on()

	if mode == 0:
		display_on = False
		print("display on")
	return

session_bus.add_message_filter(on_display_object_message)

login_object = system_bus.get_object("org.freedesktop.login1", "/org/freedesktop/login1")

def sleep_handler(*args, **kwargs):
	global preparing_to_sleep
	if args[0]:
		preparing_to_sleep = True
	else:
		preparing_to_sleep = False

login_object.connect_to_signal("PrepareForSleep", sleep_handler, sender_keyword="sender", destination_keyword="destination", interface_keyword="interface", member_keyword="member", path_keyword="path")

GLib.MainLoop().run()
