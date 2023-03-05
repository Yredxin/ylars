#include "ylars_api.h"
#include <iostream>

void usage()
{
	std::cout << "usage: ./get_route [modid] [cmdid]" << std::endl;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		usage();
		exit(1);
	}
	ylars_api api;
	uint32_t modid = atoi(argv[1]);
	uint32_t cmdid = atoi(argv[2]);

	ylars_route_t route;
	api.get_route(modid, cmdid, route);
	for (auto& host : route)
	{
		std::cout << host.first << ":" << host.second << std::endl;
	}
	return 0;
}
