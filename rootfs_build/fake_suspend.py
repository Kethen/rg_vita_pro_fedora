import os
import os.path
import re
import evdev

power_key_dev_name = "rk805 pwrkey"
power_key_code = 116
backlight_wake_min = 20

user_slice_regex = re.compile('user-([0-9]+).slice')
def get_freeze_targets():
	directories = os.listdir("/sys/fs/cgroup/user.slice")
	if len(directories) == 0:
		return {}
	user_directories = {}
	for item in directories:
		match_result = user_slice_regex.match(item)
		if match_result is None:
			continue
		uid = match_result.group(1)
		#app_slice_path = "/sys/fs/cgroup/user.slice/user-{0}.slice/user@{0}.service/app.slice/cgroup.freeze".format(uid)
		#app_slice_path = "/sys/fs/cgroup/user.slice/user-{0}.slice/user@{0}.service/cgroup.freeze".format(uid)
		app_slice_path = "/sys/fs/cgroup/user.slice/user-{0}.slice/cgroup.freeze".format(uid)
		if not os.path.exists(app_slice_path):
			continue
		user_directories[uid] = app_slice_path
	return user_directories

def write_file(path, string):
	f = open(path, "w")
	f.write(string)
	f.close()

def read_file(path):
	f = open(path, "r")
	content = f.read()
	f.close
	return content

def freeze_apps():
	for uid, path in get_freeze_targets().items():
		write_file(path, "1")

def unfreeze_apps():
	for uid, path in get_freeze_targets().items():
		write_file(path, "0")

def open_evdevs():
	devices = {}
	for path in evdev.list_devices():
		dev = evdev.InputDevice(path)
		devices[path] = dev
	return devices

def find_device_by_name(devices, name):
	for path, dev in devices.items():
		if dev.name == name:
			return dev
	return None

def grab_devices(devices, grab):
	devices = {}
	for path, dev in devices.items():
		if grab:
			dev.grab()
		else:
			dev.ungrab()

def remove_device(devices, remove_dev):
	for path, dev in devices.items():
		if dev == remove_dev:
			del devices[path]
			return

backlight_brightness = {}
def backlight_control(on):
	for entry in os.listdir("/sys/class/backlight"):
		brightness_path = "/sys/class/backlight/{0}/brightness".format(entry)
		if not os.path.exists(brightness_path):
			continue

		to_write = "{0}".format(backlight_wake_min)

		if on:
			if brightness_path in backlight_brightness:
				to_write = backlight_brightness[brightness_path]
				if int(to_write) < backlight_wake_min:
					to_write = "{0}".format(backlight_wake_min)
		else:
			backlight_brightness[brightness_path] = read_file(brightness_path)
			to_write = "0"

		write_file(brightness_path, to_write)

led_brightness = {}
def led_control(on):
	for entry in os.listdir("/sys/class/leds"):
		brightness_path = "/sys/class/leds/{0}/brightness".format(entry)
		if not os.path.exists(brightness_path):
			continue

		to_write = "0"

		if on:
			if brightness_path in led_brightness:
				to_write = led_brightness[brightness_path]
		else:
			led_brightness[brightness_path] = read_file(brightness_path)
			to_write = "0"

		write_file(brightness_path, to_write)

def get_cpu_info(cpu_num):
	cpu_path = "/sys/devices/system/cpu/cpu{0}".format(cpu_num)
	cpu_min_freq_path = "{0}/cpufreq/cpuinfo_min_freq".format(cpu_path)
	cpu_max_freq_path = "{0}/cpufreq/cpuinfo_max_freq".format(cpu_path)

	return {
		"min_freq":read_file(cpu_min_freq_path),
		"max_freq":read_file(cpu_max_freq_path)
	}

def cpu_level(up):
	cpu_num = 1
	while True:
		cpu_path = "/sys/devices/system/cpu/cpu{0}".format(cpu_num)
		cpu_online_path = "{0}/online".format(cpu_path)
		to_write = "0"
		if up:
			to_write = "1"
		try:
			write_file(cpu_online_path, to_write)
		except:
			print("stopped cpu 1 - {0}".format(cpu_num - 1))
			break
		cpu_num = cpu_num + 1

	cpu_info = get_cpu_info(0)
	cpu_path = "/sys/devices/system/cpu/cpu0"
	cpu_scaling_max_freq_path = "{0}/cpufreq/scaling_max_freq".format(cpu_path)
	to_write = cpu_info["min_freq"]
	if up:
		to_write = cpu_info["max_freq"]

	write_file(cpu_scaling_max_freq_path, to_write)

radio = {}
def radio_state(up):
	for entry in os.listdir("/sys/class/rfkill"):
		soft_kill_path = "/sys/class/rfkill/{0}/soft".format(entry)
		if not os.path.exists(soft_kill_path):
			continue

		to_write = "0"
		if up:
			if soft_kill_path in radio:
				to_write = radio[soft_kill_path]
		else:
			radio[soft_kill_path] = read_file(soft_kill_path)
			to_write = "1"

		write_file(soft_kill_path, to_write)

# daemon mode
suspended = False
def toggle_suspend(skip_backlight, devices):
	global suspended
	if suspended:
		print("resuming")
		grab_devices(devices, False)
		if not skip_backlight:
			backlight_control(True)
		led_control(True)
		cpu_level(True)
		radio_state(True)
		unfreeze_apps()
	else:
		print("suspending")
		grab_devices(devices, True)
		if not skip_backlight:
			backlight_control(False)
		led_control(False)
		cpu_level(False)
		radio_state(False)
		freeze_apps()
	suspended = not suspended

def daemon_mode(devices, power_key_dev):
	power_key_dev.grab()
	for event in power_key_dev.read_loop():
		if event.code == power_key_code and event.value == 0:
			toggle_suspend(False, devices)

def oneshot_mode(devices, power_key_dev):
	power_key_dev.grab()
	toggle_suspend(False, devices)
	for event in power_key_dev.read_loop():
		if event.code == power_key_code and event.value == 0:
			toggle_suspend(False, devices)
			power_key_dev.ungrab()
			break

devices = open_evdevs()
power_key_dev = find_device_by_name(devices, power_key_dev_name)

if power_key_dev is None:
	print("power key not found")
	exit(1)

remove_device(devices, power_key_dev)

oneshot_mode(devices, power_key_dev)
