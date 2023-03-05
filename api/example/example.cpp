#include "ylars_api.h"
#include <iostream>
int main()
{
	ylars_api api;
	uint32_t modid = 1;
	uint32_t cmdid = 1;
	std::string ip;
	uint16_t port = 0;
	int ret = api.get_host(modid, cmdid, ip, port);
	if (ret == 0)
	{
		std::cout << ip << ":" << port << std::endl;
	}
	else if (ret == 1)
	{
		std::cout << "超载，没有可用主机" << std::endl;
	}
	else if (ret == 2)
	{
		std::cout << "系统错误" << std::endl;
	}
	else if (ret == 3)
	{
		std::cout << "没有注册任何主机" << std::endl;
	}
	api.report(modid, cmdid, ip, port, (ret == 1));
	return 0;
}
