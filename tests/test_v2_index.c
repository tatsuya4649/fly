#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "v2.h"


int main()
{
	uint8_t test;

	/* test index header */
	printf("Test index header\n");
	for (int i=0; i<=255; i++){
		printf("%d\n", i);
		test = i;
		if (i >= 128 && i <= 255)
			assert(fly_hv2_is_index_header_field(&test) == true);
		else
			assert(fly_hv2_is_index_header_field(&test) == false);
	}
	printf("Passed: Test index header\n");

	/* test literal header field with incremental indexing */
	printf("Test literal header field with incremental indexing\n");
	for (int i=0; i<=255; i++){
		printf("%d\n", i);
		test = i;
		if (i >= 64 && i <= 127)
			assert(fly_hv2_is_index_header_update(&test) == true);
		else
			assert(fly_hv2_is_index_header_update(&test) == false);
	}
	printf("Passed: Test literal header field with incremental indexing\n");

	/* test literal header field without indexing */
	printf("Test literal header field without indexing\n");
	for (int i=0; i<=255; i++){
		printf("%d\n", i);
		test = i;
		if (i >= 0 && i <= 15)
			assert(fly_hv2_is_index_header_noupdate(&test) == true);
		else
			assert(fly_hv2_is_index_header_noupdate(&test) == false);
	}
	printf("Passed: Test literal header field without indexing\n");

	/* test literal header field never indexing */
	printf("Test literal header field never indexing\n");
	for (int i=0; i<=255; i++){
		printf("%d\n", i);
		test = i;
		if (i >= 16 && i <= 31)
			assert(fly_hv2_is_index_header_noindex(&test) == true);
		else
			assert(fly_hv2_is_index_header_noindex(&test) == false);
	}
	printf("Passed: Test literal header field never indexing\n");
}
