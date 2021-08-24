#ifdef __cplusplus
extern "C"{
	#include "method.h"
}
#endif

TEST(METHOD, fly_match_method_name)
{
	EXPECT(fly_match_method_name("get") != NULL);
	EXPECT(fly_match_method_name("GET") != NULL);
	EXPECT(fly_match_method_name("geT") != NULL);
	EXPECT(fly_match_method_name("METHOD") == NULL);
	EXPECT(fly_match_method_name(NULL) == NULL);
}

TEST(METHOD, fly_match_method_type)
{
	EXPECT(fly_match_method_type(GET) != NULL);
	EXPECT(fly_match_method_type(1000) == NULL);
}
