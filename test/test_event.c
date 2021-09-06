#include <assert.h>
#include <stdio.h>
#include "event.h"
#include "util.h"

int handler(__unused int fd)
{
	printf("Handler\n");
}

int main()
{
	fly_event_manager_t *manager;
	fly_event_t *event;

	manager = fly_event_manager_init();
	assert(manager != NULL);

	event = fly_event_init(manager);
	assert(event != NULL);

	assert(fly_event_timer_init(event) == 0);
	assert(fly_event_register(event) == 0);

	assert(fly_event_handler(manager) == 0);

	assert(fly_event_manager_release(manager) != -1);
}
