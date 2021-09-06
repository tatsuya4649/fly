#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include "event.h"
#include "util.h"

#define TEST_MOUSE			"/dev/input/mice"

int handler(__unused int fd)
{
	printf("Handler\n");
}

int main()
{
	fly_event_manager_t *manager;
	fly_event_t *event;
	int fd;

	fd = open(TEST_MOUSE, O_RDONLY);
	if (fd == -1){
		perror("open");
		return 1;
	}

	manager = fly_event_manager_init();
	assert(manager != NULL);

	event = fly_event_init(manager);
	assert(event != NULL);

	event->fd = fd;
	event->read_or_write = FLY_READ;
	event->handler = handler;
	assert(fly_event_timer_init(event) == 0);
	assert(fly_event_register(event) == 0);

	assert(fly_event_handler(manager) == 0);

	assert(fly_event_manager_release(manager) != -1);
}
