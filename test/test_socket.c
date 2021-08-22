#include "CppUTest/CommandLineTestRunner.h"
#include <iostream>
#include "test_socket.h"

TEST_GROUP(test_socket){
	TEST_SETUP()
	{
		std::cout << "Start Socket Test" << std::endl;
	}
	TEST_TEARDOWN()
	{
		std::cout << "End Socket Test" << std::endl;
	}
};

TEST(test_socket, init)
{
	std::cout << "init_test" << std::endl;
}
