#include "util.h"

int fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr)
{
	while(*src != '\0'){
		if (limit_addr != NULL && src >= limit_addr)
			return -1;

		if (target != NULL){
			for (size_t i=0;i<strlen(target); i++){
				if (target[i] == *src){
					dist[i] = '\0';
					return 0;
				}
			}
		}

		*dist++ = *src++;
	}
	return 1;
}
