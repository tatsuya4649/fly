#include "math.h"

int fly_number_digits(int digit)
{
	int i=0;
	while (digit!=0){
		digit = digit/10;
		i++;
	}
	return i;
}
