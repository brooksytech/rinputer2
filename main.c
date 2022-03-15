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
	int infd;
	int isUsed;
	pthread_t thread;
	struct rinputer_device *next;
};

void *worker(void *data)
{
	struct rinputer_device *my_device = (struct rinputer_device*)data;

	printf("Inside thread, we are doing %s\n", my_device->path);

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
