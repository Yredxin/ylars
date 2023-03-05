#include <ylars_api.h>
#include <iostream>
#include <unistd.h>
#include <map>
#include <string>

void usage()
{
	std::cout << "usage: ./simulator modid cmdid [errRate(0-10) [query cnt (0-999999)]]" << std::endl;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		usage();
		exit(1);
	}
	uint32_t modid = atoi(argv[1]);
	uint32_t cmdid = atoi(argv[2]);
	int err_rate = 2; // 20%错误率
	int query_cnt = 100;
	if (argc >= 4)
	{
		err_rate = atoi(argv[3]);
	}
	if (argc == 5)
	{
		query_cnt = atoi(argv[4]);
	}

	// 统计所有的ip
	std::map<std::string, std::pair<int, int>> result;
	ylars_api api;
	// 初始化
	if (api.reg_init(modid, cmdid) != 0)
	{
		std::cout << "no exist" << std::endl;
		exit(1);
	}
	std::string ip;
	uint16_t port;
	int ret;
	for (int i = 0; i < query_cnt; i++)
	{
		ret = api.get_host(modid, cmdid, ip, port);
		if (result.find(ip) == result.end())
		{
			// 首次出现
			result[ip] = std::pair<int, int>{ 0,0 };
		}
		std::cout << "host :" << ip << " host called ";
		if (ret == 0)
		{
			if (rand() % 10 < err_rate)
			{
				// 失败
				api.report(modid, cmdid, ip, port, false);
				result[ip].second++;
				std::cout << " ERROR!!!" << std::endl;
			}
			else
			{
				// 成功
				api.report(modid, cmdid, ip, port, true);
				result[ip].first++;
				std::cout << " SUCC." << std::endl;
			}
		}
		else if (ret == 3)
		{
			// 资源不存在
			std::cout << modid << "," << cmdid << " not exist" << std::endl;
		}
		else if (ret == 1)
		{
			// 负载
			std::cout << modid << "," << cmdid << " all hosts overload!!" << std::endl;
		}
		else if (ret == 2)
		{
			// 系统错误
			std::cout << " lars system error!!" << std::endl;
			break;
		}

		usleep(5000);
	}

	for (auto& res : result)
	{
		std::cout << res.first
			<< " ==> succ : " << res.second.first
			<< " <=> err : " << res.second.second << std::endl;
	}
	return 0;
}