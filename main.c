#include <stdio.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>

int outfd = -1;

static int abs_top = 1024;
static int abs_bot = -1024;

struct rinputer_device
{
	char *path;
	char *name;
	int infd;
	int isUsed;
	pthread_t thread;
	struct rinputer_device *next;
};

int map(int x, int in_min, int in_max, int out_min, int out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void emit(int type, int code, int value)
{
	if(outfd < 0)
		return;

	struct input_event ev;

	ev.type = type;
	ev.code = code;
	ev.value = value;

	ev.time.tv_sec = 0;
	ev.time.tv_usec = 0;

	write(outfd, &ev, sizeof(ev));
}

void emit_abs(signed int min, signed int max, int code, int value)
{
	int newval = map(value, min, max, abs_bot, abs_top);
	emit(EV_ABS, code, newval);
}

void *worker(void *data)
{
	struct rinputer_device *my_device = (struct rinputer_device*)data;
	int min = 0; // to be filled in later
	int max = 0;

	my_device->isUsed = 0;

	// we only do gamepaddish devices, sort out everything else
	int useful = 0;
	int touchscreen = 0;
	char types[EV_MAX];
	char codes[KEY_MAX/8 + 1];
	
	memset(types, 0, EV_MAX);
	memset(codes, 0, sizeof(codes));
	ioctl(my_device->infd, EVIOCGBIT(0, EV_MAX), &types);
	
	for(int i = 0; i < EV_MAX; i++)
	{
		if((types[0] >> i) & 1)
		{
			switch(i)
			{
				case EV_KEY:
					ioctl(my_device->infd, EVIOCGBIT(EV_KEY, sizeof(codes)), &codes);
					
					// dividing by 8 because all values
					// are stored as bits, not bytes
					if((codes[BTN_SOUTH / 8]) & 1)
						useful = 1;

					// technically rounds down to check for
					// anything between 0x148 to 0x14f
					// but it works out in our favour
					// anyway
					if(codes[BTN_TOUCH / 8])
						touchscreen = 1;
					break;
				case EV_ABS:
					ioctl(my_device->infd, EVIOCGBIT(EV_ABS, sizeof(codes)), &codes);
					if((codes[ABS_X / 8]) & 1)
						useful = 1;

					struct input_absinfo tmp;
					ioctl(my_device->infd, EVIOCGABS(ABS_X), &tmp);

					min = tmp.minimum;
					max = tmp.maximum;

					break;
			}
		}
	}
	if(useful == 1 && touchscreen == 0)
		printf("device \"%s\" deemed useful \n", my_device->name);
	else
		goto out;


	ioctl(my_device->infd, EVIOCGRAB, 1);

	int rd = 0;
	int i = 0;
	struct input_event ev[4];
	while(1)
	{
		rd = read(my_device->infd, ev, sizeof(struct input_event) * 4);
		if(rd > 0)
		{
			for(i = 0; i < rd / sizeof(struct input_event) * 4; i++)
			{
				if(ev[i].type == EV_ABS)
					emit_abs(min, max, ev[i].code, ev[i].value);
				else
					emit(ev[i].type, ev[i].code, ev[i].value);
			}
		}
	}

out:
	close(my_device->infd);
	free(data);
	return NULL;
}

void attach_node(struct rinputer_device *head, struct rinputer_device *new)
{
	head->next = new;
	new->next = 0;
}

int rescan_devices(struct rinputer_device *head)
{
	struct rinputer_device *tmpdev;
	char dev[20];
	char name[33];
	name[32] = '\0';
	int tmpfd;

	for(int i = 0; i < 64; i++)
	{
		sprintf(dev, "/dev/input/event%d", i);
		tmpfd = open(dev, O_RDONLY);
		if(ioctl(tmpfd, EVIOCGNAME(32), name) < 0)
			continue;

		// let's not make a loop
		if(strncmp("Rinputer", name, 8) == 0)
			continue;

		printf("Found potential input device: %s\n", name);
		tmpdev = calloc(1, sizeof(struct rinputer_device));
		tmpdev->path = malloc(strlen(dev) + 1);
		strcpy(tmpdev->path, dev);
		tmpdev->name = malloc(strlen(name) + 1);
		strcpy(tmpdev->name, name);
		tmpdev->infd = tmpfd;
		tmpdev->next = head->next;
		head->next = tmpdev;
		pthread_create(&tmpdev->thread, NULL, worker, tmpdev);
	}
	return 0;
}

void setup_abs(int fd, unsigned int chan)
{
	ioctl(fd, UI_SET_ABSBIT, chan);

	struct uinput_abs_setup tmp =
	{
		.code = chan,
		.absinfo = {
			.minimum = abs_bot,
			.maximum = abs_top
		}
	};

	ioctl(fd, UI_ABS_SETUP, &tmp);
}

int main(void)
{
	int ret;
	struct rinputer_device *head = malloc(sizeof(struct rinputer_device));
	head->isUsed = 0;
	head->next = 0;

	outfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(outfd < 0)
		perror("Error opening /dev/uinput");

	struct uinput_setup usetup;

	ioctl(outfd, UI_SET_EVBIT, EV_KEY);

	ioctl(outfd, UI_SET_KEYBIT, BTN_DPAD_UP);	// dpad up
	ioctl(outfd, UI_SET_KEYBIT, BTN_DPAD_DOWN);	// dpad down
	ioctl(outfd, UI_SET_KEYBIT, BTN_DPAD_LEFT);	// dpad left
	ioctl(outfd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);	// dpad right

	ioctl(outfd, UI_SET_KEYBIT, BTN_NORTH);		// x
	ioctl(outfd, UI_SET_KEYBIT, BTN_SOUTH);		// b
	ioctl(outfd, UI_SET_KEYBIT, BTN_WEST);		// y
	ioctl(outfd, UI_SET_KEYBIT, BTN_EAST);		// a

	ioctl(outfd, UI_SET_KEYBIT, BTN_TL);		// L1
	ioctl(outfd, UI_SET_KEYBIT, BTN_TR);		// R1
	
	ioctl(outfd, UI_SET_KEYBIT, BTN_TR2);		// L2
	ioctl(outfd, UI_SET_KEYBIT, BTN_TL2);		// R2

	ioctl(outfd, UI_SET_KEYBIT, BTN_SELECT);
	ioctl(outfd, UI_SET_KEYBIT, BTN_START);

	ioctl(outfd, UI_SET_KEYBIT, BTN_MODE);		// menu

	ioctl(outfd, UI_SET_EVBIT, EV_ABS);

	setup_abs(outfd, ABS_X);
	setup_abs(outfd, ABS_Y);
	setup_abs(outfd, ABS_RX);
	setup_abs(outfd, ABS_RY);

	// maybe we should pretend to be xbox gamepad?
	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234;
	usetup.id.product = 0x5678;
	strcpy(usetup.name, "Rinputer");

	ioctl(outfd, UI_DEV_SETUP, &usetup);
	ioctl(outfd, UI_DEV_CREATE);

	ret = rescan_devices(head);
	if(ret)
		return 1;

	while(1)
		sleep(10); // rescanning goes here

	return 0;
}
