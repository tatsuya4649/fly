#include <assert.h>
#include <stdlib.h>
#include "config.h"

int main()
{
	__unused int res;
	if (setenv(FLY_CONFIG_PATH_ENV, "test/test.conf", 1) == -1)
		return -1;

	printf("-* Before *-\n");
	for (struct fly_config *__c=configs; __c->name; __c++)
		printf("%s: %s\n", __c->env_name, getenv(__c->env_name));

	res = fly_parse_config_file(NULL);

	printf("-* After *-\n");
	for (struct fly_config *__c=configs; __c->name; __c++)
		printf("%s: %s\n", __c->env_name, getenv(__c->env_name));
}
