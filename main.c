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
	DIR *d;
	struct dirent *ent;
	char *wholePath;
	struct rinputer_device *tmpdev;

	d = opendir("/dev/input/by-path/");
	if(d)
	{
		while((ent = readdir(d)) != NULL)
		{
			if(ent->d_type == DT_CHR || ent->d_type == DT_LNK)
			{
				printf("Found potential input device: %s\n", ent->d_name);
				
				tmpdev = calloc(1, sizeof(struct rinputer_device));

				tmpdev->path = malloc(strlen(ent->d_name) + strlen("/dev/input/by-path/"));
				sprintf(tmpdev->path, "/dev/input/by-path/%s", ent->d_name);
				
				tmpdev->next = head->next;
				head->next = tmpdev;

				pthread_create(&tmpdev->thread, NULL, worker, tmpdev);
			}
		}
	}
	else
	{
		perror("Failed opening /dev/input/by-path");
		return 1;
	}
	free(d);
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
