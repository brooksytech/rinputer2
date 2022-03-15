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

int outfd;

struct rinputer_device
{
	char *path;
	char *name;
	int infd;
	int isUsed;
	pthread_t thread;
	struct rinputer_device *next;
};

void *worker(void *data)
{
	struct rinputer_device *my_device = (struct rinputer_device*)data;
	
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
					break;
			}
		}
	}
	if(useful == 1 && touchscreen == 0)
		printf("device \"%s\" deemed useful \n", my_device->name);

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

int main(void)
{
	int ret;
	struct rinputer_device *head = malloc(sizeof(struct rinputer_device));
	head->isUsed = 0;
	head->next = 0;

	ret = rescan_devices(head);
	if(ret)
		return 1;
}
