#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>

#include <thread>
#include <mutex>
#include <string>
#include <filesystem>

#define CALIBRATION_FILE "/etc/vita_controller_calibration"
#define CALIBRATION_FOLD 10
#define CALIBRATION_SLOTS ((360 / CALIBRATION_FOLD) + 1)

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

void save_calibration(std::string path, const struct abs_calibration *calibration_l, const struct abs_calibration *calibration_r){
	FILE *file = fopen(path.c_str(), "wb");
	if (file == NULL){
		printf("%s: failed opening %s for writing\n", __func__, path.c_str());
		exit(1);
	}

	for (int i = 0;i < CALIBRATION_SLOTS;i++){
		int write_status = fwrite(&calibration_l[i].max, sizeof(calibration_l[i].max), 1, file);
		if (write_status != 1){
			printf("%s: failed writing to %s\n", __func__, path.c_str());
			exit(1);
		}
	}

	for (int i = 0;i < CALIBRATION_SLOTS;i++){
		int write_status = fwrite(&calibration_r[i].max, sizeof(calibration_r[i].max), 1, file);
		if (write_status != 1){
			printf("%s: failed writing to %s\n", __func__, path.c_str());
			exit(1);
		}
	}

	fclose(file);
}

bool read_calibration(std::string path, struct abs_calibration *calibration_l, struct abs_calibration *calibration_r){
	FILE *file = fopen(path.c_str(), "rb");
	if (file == NULL){
		printf("%s: failed opening %s for reading\n", __func__, path.c_str());
		return false;
	}

	for (int i = 0;i < CALIBRATION_SLOTS;i++){
		int read_status = fread(&calibration_l[i].max, sizeof(calibration_l[i].max), 1, file);
		if (read_status != 1){
			printf("%s: failed reading from %s\n", __func__, path.c_str());
			fclose(file);
			return false;
		}
	}

	for (int i = 0;i < CALIBRATION_SLOTS;i++){
		int read_status = fread(&calibration_r[i].max, sizeof(calibration_r[i].max), 1, file);
		if (read_status != 1){
			printf("%s: failed reading from %s\n", __func__, path.c_str());
			fclose(file);
			return false;
		}
	}

	fclose(file);
	return true;
}

void get_hypotenuse_slot_rad_width_height(double &hypotenuse, int &slot, double &rad, double &width, double &height, struct input_absinfo &info_x, struct input_absinfo &info_y, int x, int y){
	width = x - info_x.value;
	height = y - info_y.value;
	hypotenuse = sqrt(pow(width, 2) + pow(height, 2));
	rad = asin(height / hypotenuse);
	slot = (rad * 180) / M_PI;

	if (width < 0 && height >= 0){
		// top left
		slot = 90 + (90 - slot);
	}else if (width < 0 && height < 0){
		// bottom left
		slot = 180 + slot * -1;
	}else if (width >= 0 && height < 0){
		// bottom right
		slot = 270 + (90 - slot * -1);
	}
	if (slot > 360){
		slot = 360;
	}
	if (slot < 0){
		slot = 0;
	}

	slot = slot / CALIBRATION_FOLD;
}

bool would_block(){
	return errno == EWOULDBLOCK || errno == EAGAIN;
}

void print_calibration(struct abs_calibration *calibration){
	for (int i = 0;i < CALIBRATION_SLOTS;i++){
		printf("%d %d\n", i, calibration[i].max);
	}
}

void calibrate(int joystick_fd){
	struct input_absinfo info_lx = {0};
	struct input_absinfo info_ly = {0};
	struct input_absinfo info_rx = {0};
	struct input_absinfo info_ry = {0};
	ioctl(joystick_fd, EVIOCGABS(ABS_X), &info_lx);
	ioctl(joystick_fd, EVIOCGABS(ABS_Y), &info_ly);
	ioctl(joystick_fd, EVIOCGABS(ABS_RX), &info_rx);
	ioctl(joystick_fd, EVIOCGABS(ABS_RY), &info_ry);

	static struct abs_calibration calibration_l[CALIBRATION_SLOTS] = {0};
	static struct abs_calibration calibration_r[CALIBRATION_SLOTS] = {0};

	int lx = info_lx.value;
	int ly = info_ly.value;
	int rx = info_rx.value;
	int ry = info_ry.value;

	printf("%s: now slowly and gently rotate the analog sticks, when done, press enter\n", __func__);

	struct input_event event = {0};
	while (true){
		struct pollfd pfd[2] = {0};
		pfd[0].fd = joystick_fd;
		pfd[1].fd = 1;
		pfd[0].events = POLLIN;
		pfd[1].events = POLLIN;
		poll(pfd, 2, -1);

		if (pfd[1].revents) {
			break;
		}

		if (!(pfd[0].revents | POLLIN)){
			printf("%s: device poll error!\n", __func__);
			exit(1);
		}

		int read_status = read(joystick_fd, &event, sizeof(event));
		if (read_status != sizeof(event)){
			printf("%s: device read error!\n", __func__);
			exit(1);
		}

		if (event.type != EV_ABS){
			continue;
		}
		switch(event.code){
			case ABS_X:
				lx = event.value;
				break;
			case ABS_Y:
				ly = event.value;
				break;
			case ABS_RX:
				rx = event.value;
				break;
			case ABS_RY:
				ry = event.value;
				break;
		}

		double hypotenuse_l;
		double hypotenuse_r;
		int slot_l;
		int slot_r;
		double rad_l;
		double rad_r;
		double width_l;
		double width_r;
		double height_l;
		double height_r;

		get_hypotenuse_slot_rad_width_height(hypotenuse_l, slot_l, rad_l, width_l, height_l, info_lx, info_ly, lx, ly);
		get_hypotenuse_slot_rad_width_height(hypotenuse_r, slot_r, rad_r, width_r, height_r, info_rx, info_ry, rx, ry);

		int hypotenuse_l_int = hypotenuse_l;
		int hypotenuse_r_int = hypotenuse_r;
		if (hypotenuse_l_int > 32767){
			hypotenuse_l_int = 32767;
		}
		if (hypotenuse_r_int > 32767){
			hypotenuse_r_int = 32767;
		}

		if (calibration_l[slot_l].max < hypotenuse_l_int){
			calibration_l[slot_l].max = hypotenuse_l_int;
		}

		if (calibration_r[slot_r].max < hypotenuse_r_int){
			calibration_r[slot_r].max = hypotenuse_r_int;
		}
	}

	print_calibration(calibration_l);
	print_calibration(calibration_r);

	save_calibration(CALIBRATION_FILE, calibration_l, calibration_r);

	printf("%s: calibration done\n", __func__);
}



int convert(struct abs_calibration *calibration, struct input_absinfo &info_x, struct input_absinfo &info_y, int x, int y, bool ret_x, bool otf_calibration){
	const static double amplification = 1.1;

	double hypotenuse;
	int slot;
	double rad;
	double width;
	double height;
	get_hypotenuse_slot_rad_width_height(hypotenuse, slot, rad, width, height, info_x, info_y, x, y);

	int hypotenuse_int = hypotenuse;
	if (hypotenuse_int > 32767){
		hypotenuse_int = 32767;
	}
	if (otf_calibration){
		if (hypotenuse_int > calibration[slot].max){
			calibration[slot].max = hypotenuse;
		}
	}

	int adjusted_hypotenuse = ((hypotenuse_int * 32767 * amplification) / calibration[slot].max);

	//printf("%s: %d %d %d\n", __func__, hypotenuse_int, adjusted_hypotenuse, calibration[slot].max);

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
	}
	if (new_width < -32768){
		new_width = -32768;
	}
	if (new_width > 32767){
		new_width = 32767;
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

	static struct abs_calibration calibration_l[CALIBRATION_SLOTS] = {0};
	static struct abs_calibration calibration_r[CALIBRATION_SLOTS] = {0};

	bool has_calibration_file = read_calibration(CALIBRATION_FILE, calibration_l, calibration_r);
	if (!has_calibration_file){
		printf("%s: no calibration file loaded, calibrating on the fly\n", __func__);
	} else {
		printf("%s: calibration file loaded\n", __func__);
		print_calibration(calibration_l);
		print_calibration(calibration_r);
	}

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
				CONV(ABS_X, EV_ABS, ABS_X, lx = event.value, convert(calibration_l, info_lx, info_ly, lx, ly, true, !has_calibration_file))
				CONV(ABS_Y, EV_ABS, ABS_Y, ly = event.value, convert(calibration_l, info_lx, info_ly, lx, ly, false, !has_calibration_file))
				CONV(ABS_RX, EV_ABS, ABS_RX, rx = event.value, convert(calibration_r, info_rx, info_ry, rx, ry, true, !has_calibration_file))
				CONV(ABS_RY, EV_ABS, ABS_RY, ry = event.value, convert(calibration_r, info_rx, info_ry, rx, ry, false, !has_calibration_file))
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

int main(int argc, char **argv){
	int uinput_fd = setup_uinput();
	std::mutex uinput_fd_mutex;
	int button_fd = open_device("gamepad-keys");
	int joystick_fd = open_device("MCU Joypad");

	if (argc >= 2){
		if (strcmp("calibrate", argv[1]) == 0){
			calibrate(joystick_fd);
			return 0;
		}else{
			printf("usage:\n");
			printf("driver mode: %s\n", argv[0]);
			printf("calibration mode: %s calibration\n", argv[0]);
			exit(1);
		}
	}

	auto button_poller_thread = std::thread(button_poller, button_fd, uinput_fd, std::ref(uinput_fd_mutex));
	auto joystick_poller_thread = std::thread(joystick_poller, joystick_fd, uinput_fd, std::ref(uinput_fd_mutex));
	button_poller_thread.join();

	return 0;
}
