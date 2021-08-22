#include "util.h"

void fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr)
{
	while(*src != '\0'){
		if (src >= limit_addr)
			return;
		for (size_t i=0;i<strlen(target); i++){
			if (target[i] == *src){
				dist[i+1] = '\0';
				return;
			}
		}

		*dist++ = *src++;
	}
	return;
}
