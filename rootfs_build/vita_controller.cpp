#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>

#include <thread>
#include <mutex>
#include <string>
#include <filesystem>

int setup_uinput(){
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0){
		printf("%s: failed opening /dev/uinput, %d %s\n", __func__,  errno, strerror(errno));
		exit(1);
	}

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	#define ADD_KEY(key) { \
		ioctl(fd, UI_SET_KEYBIT, key); \
	}
	ADD_KEY(BTN_SOUTH);
	ADD_KEY(BTN_EAST);
	ADD_KEY(BTN_NORTH);
	ADD_KEY(BTN_WEST);
	ADD_KEY(BTN_TL);
	ADD_KEY(BTN_TR);
	ADD_KEY(BTN_SELECT);
	ADD_KEY(BTN_START);
	ADD_KEY(BTN_MODE);
	ADD_KEY(BTN_THUMBL);
	ADD_KEY(BTN_THUMBR);
	#undef ADD_KEY

	ioctl(fd, UI_SET_EVBIT, EV_ABS);
	#define ADD_ABS(abs, min, max) { \
		ioctl(fd, UI_SET_ABSBIT, abs); \
		struct uinput_abs_setup setup = {0}; \
		setup.absinfo.minimum = min; \
		setup.absinfo.maximum = max; \
		setup.code = abs; \
		ioctl(fd, UI_ABS_SETUP, &setup); \
	}
	ADD_ABS(ABS_X, -32768, 32767);
	ADD_ABS(ABS_Y, -32768, 32767);
	ADD_ABS(ABS_Z, 0, 255);
	ADD_ABS(ABS_RX, -32768, 32767);
	ADD_ABS(ABS_RY, -32768, 32767);
	ADD_ABS(ABS_RZ, 0, 255);
	ADD_ABS(ABS_HAT0X, -1, 1);
	ADD_ABS(ABS_HAT0Y, -1, 1);
	#undef ADD_ABS

	struct uinput_setup setup = {0};
	setup.id.bustype = BUS_USB;
	setup.id.vendor = 0x045e;
	setup.id.product = 0x028e;
	strcpy(setup.name, "rg vita combined controller");

	ioctl(fd, UI_DEV_SETUP, &setup);
	ioctl(fd, UI_DEV_CREATE);

	return fd;
}

int open_device(std::string name){
	for (const auto &entry : std::filesystem::directory_iterator("/dev/input")){
		char prefix[] = "/dev/input/event";
		if (strncmp(entry.path().c_str(), prefix, sizeof(prefix) - 1) != 0){
			continue;
		}
		int fd = open(entry.path().c_str(), O_RDONLY);
		if (fd < 0){
			printf("%s: failed probing %s\n", __func__, entry.path());
			continue;
		}
		char dev_name[256] = {0};
		ioctl(fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
		if (strncmp(dev_name, name.c_str(), sizeof(dev_name)) == 0){
			ioctl(fd, EVIOCGRAB, 1);
			return fd;
		}
		close(fd);
	}
	printf("%s: cannot find %s\n", __func__, name.c_str());
	exit(1);
	return 0;
}

void button_poller(int button_fd, int uinput_fd, std::mutex &uinput_fd_mutex){
	struct input_event event = {0};
	while (true){
		int read_status = read(button_fd, &event, sizeof(event));
		if (read_status != sizeof(event)){
			printf("%s: button poller failed\n", __func__);
		}
		struct input_event new_event = {0};
		bool send_event = false;
		if (event.type == EV_SYN){
			send_event = true;
			new_event = event;
		}else if (event.type == EV_KEY){
			switch(event.code){
				#define CONV(c, new_type, new_code, conv) \
				case c: \
					new_event.type = new_type; \
					new_event.code = new_code; \
					new_event.value = conv; \
					send_event = true; \
					break;
				CONV(BTN_DPAD_UP, EV_ABS, ABS_HAT0Y, event.value ? -1 : 0)
				CONV(BTN_DPAD_DOWN, EV_ABS, ABS_HAT0Y, event.value ? 1 : 0)
				CONV(BTN_DPAD_LEFT, EV_ABS, ABS_HAT0X, event.value ? -1 : 0)
				CONV(BTN_DPAD_RIGHT, EV_ABS, ABS_HAT0X, event.value ? 1 : 0)
				CONV(BTN_NORTH, EV_KEY, BTN_WEST, event.value)
				CONV(BTN_SOUTH, EV_KEY, BTN_SOUTH, event.value)
				CONV(BTN_WEST, EV_KEY, BTN_NORTH, event.value)
				CONV(BTN_EAST, EV_KEY, BTN_EAST, event.value)
				CONV(BTN_TL, EV_KEY, BTN_TL, event.value)
				CONV(BTN_TR, EV_KEY, BTN_TR, event.value)
				CONV(BTN_START, EV_KEY, BTN_START, event.value)
				CONV(BTN_SELECT, EV_KEY, BTN_SELECT, event.value)
				CONV(BTN_THUMBL, EV_KEY, BTN_THUMBL, event.value)
				CONV(BTN_THUMBR, EV_KEY, BTN_THUMBR, event.value)
				CONV(BTN_MODE, EV_KEY, BTN_MODE, event.value)
				CONV(BTN_TL2, EV_ABS, ABS_Z, event.value ? 255 : 0)
				CONV(BTN_TR2, EV_ABS, ABS_RZ, event.value ? 255 : 0)
				#undef CONV
			}
		}
		if (send_event){
			uinput_fd_mutex.lock();
			int write_status = write(uinput_fd, &new_event, sizeof(new_event));
			if (write_status != sizeof(new_event)){
				printf("%s: failed writing to uinput\n", __func__);
				exit(1);
			}
			uinput_fd_mutex.unlock();
		}
	}
}

struct abs_calibration{
	int max;
};

int convert(struct abs_calibration *calibration, struct input_absinfo &info_x, struct input_absinfo & info_y, int x, int y, bool ret_x){
	double width = x - info_x.value;
	double height = y - info_y.value;
	double hypotenuse = sqrt(pow(width, 2) + pow(height, 2));
	double rad = asin(height / hypotenuse);
	int deg = (rad * 180) / M_PI;

	if (width < 0 && height >= 0){
		// top left
		deg = 90 + (90 - deg);
	}else if (width < 0 && height < 0){
		// bottom left
		deg = 180 + deg * -1;
	}else if (width >= 0 && height < 0){
		// bottom right
		deg = 270 + (90 - deg * -1);
	}
	if (deg > 360){
		deg = 360;
	}
	if (deg < 0){
		deg = 0;
	}

	//printf("%s: %d\n", __func__, deg);

	int hypotenuse_int = hypotenuse;
	if (hypotenuse_int > 32767){
		hypotenuse_int = 32767;
	}
	if (hypotenuse_int > calibration[deg].max){
		calibration[deg].max = hypotenuse;
	}

	int adjusted_hypotenuse = (hypotenuse_int * 32767) / calibration[deg].max;

	//printf("%s: %d %d %d\n", __func__, hypotenuse_int, adjusted_hypotenuse, calibration[deg].max);

	if (!ret_x){
		int new_height = sin(rad) * adjusted_hypotenuse;
		if (new_height < -32768){
			new_height = -32768;
		}
		if (new_height > 32767){
			new_height = 32767;
		}
		return new_height;
	}
	int new_width = cos(rad) * adjusted_hypotenuse;
	if (width < 0){
		new_width = new_width * -1;
		if (new_width < -32768){
			new_width = -32768;
		}else{
			if (new_width > 32767){
				new_width = 32767;
			}
		}
	}
	return new_width;
}


void joystick_poller(int joystick_fd, int uinput_fd, std::mutex &uinput_fd_mutex){
	struct input_absinfo info_lx = {0};
	struct input_absinfo info_ly = {0};
	struct input_absinfo info_rx = {0};
	struct input_absinfo info_ry = {0};
	ioctl(joystick_fd, EVIOCGABS(ABS_X), &info_lx);
	ioctl(joystick_fd, EVIOCGABS(ABS_Y), &info_ly);
	ioctl(joystick_fd, EVIOCGABS(ABS_RX), &info_rx);
	ioctl(joystick_fd, EVIOCGABS(ABS_RY), &info_ry);

	int lx = info_lx.value;
	int ly = info_ly.value;
	int rx = info_rx.value;
	int ry = info_ry.value;

	struct abs_calibration calibration_l[360] = {0};
	struct abs_calibration calibration_r[360] = {0};

	struct input_event event = {0};
	while (true){
		int read_status = read(joystick_fd, &event, sizeof(event));
		if (read_status != sizeof(event)){
			printf("%s: joystick poller failed\n", __func__);
		}
		struct input_event new_event = {0};
		bool send_event = false;
		if (event.type == EV_SYN){
			send_event = true;
			new_event = event;
		}else if (event.type == EV_ABS){
			switch(event.code){
				#define CONV(c, new_type, new_code, save, conv) \
				case c: \
					new_event.type = new_type; \
					new_event.code = new_code; \
					save; \
					new_event.value = conv; \
					send_event = true; \
					break;
				CONV(ABS_X, EV_ABS, ABS_X, lx = event.value, convert(calibration_l, info_lx, info_ly, lx, ly, true))
				CONV(ABS_Y, EV_ABS, ABS_Y, ly = event.value, convert(calibration_l, info_lx, info_ly, lx, ly, false))
				CONV(ABS_RX, EV_ABS, ABS_RX, rx = event.value, convert(calibration_r, info_rx, info_ry, rx, ry, true))
				CONV(ABS_RY, EV_ABS, ABS_RY, ry = event.value, convert(calibration_r, info_rx, info_ry, rx, ry, false))
				#undef CONV
			}
		}
		if (send_event){
			uinput_fd_mutex.lock();
			int write_status = write(uinput_fd, &new_event, sizeof(new_event));
			if (write_status != sizeof(new_event)){
				printf("%s: failed writing to uinput\n", __func__);
				exit(1);
			}
			uinput_fd_mutex.unlock();
		}
	}
}

int main(){
	int uinput_fd = setup_uinput();
	std::mutex uinput_fd_mutex;
	int button_fd = open_device("gamepad-keys");
	int joystick_fd = open_device("MCU Joypad");

	auto button_poller_thread = std::thread(button_poller, button_fd, uinput_fd, std::ref(uinput_fd_mutex));
	auto joystick_poller_thread = std::thread(joystick_poller, joystick_fd, uinput_fd, std::ref(uinput_fd_mutex));
	button_poller_thread.join();

	return 0;
}
