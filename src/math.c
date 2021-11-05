#include "math.h"

int fly_number_digits(int digit)
{
	int i=0;
	if (digit == 0)
		return 1;
	while (digit!=0){
		digit = digit/10;
		i++;
	}
	return i;
}

int fly_number_ldigits(long digit)
{
	int i=0;
	if (digit == 0)
		return 1;
	while (digit!=0){
		digit = digit/10;
		i++;
	}
	return i;
}
