#include <ylars_api.h>
#include <iostream>
#include <unistd.h>
#include <map>
#include <string>

struct ID
{
	pthread_t pid;
	uint32_t modid;
	uint32_t cmdid;
};

void usage()
{
	std::cout << "usage: ./qps [thread nums]" << std::endl;
}

void* qps_main(void* args)
{
	auto id = static_cast<ID*>(args);
	uint total_qps = 0;
	uint qps_cnt = 0;
	long last_time = time(nullptr);
	long curr_time = 0;
	uint sec = 0;
	ylars_api api;
	// 初始化
	if (api.reg_init(id->modid, id->cmdid) != 0)
	{
		std::cout << "no exist" << std::endl;
		exit(1);
	}
	std::string ip;
	uint16_t port;
	int ret;
	while (true)
	{
		// 测试主机
		ret = api.get_host(id->modid, id->cmdid, ip, port);
		if (ret == 0 || ret == 1 || ret == 3)
		{
			// 成功获取数据一次
			++qps_cnt;
			if (ret == 3)
			{
				continue;
			}
			// 上报
			api.report(id->modid, id->cmdid, ip, port, (ret == 0));
		}
		else
		{
			// 系统错误
			printf("[%d,%d] get error ret = %d\n", id->modid, id->cmdid, ret);
		}

		// 获取当前时间
		curr_time = time(nullptr);
		if (curr_time - last_time >= 1)
		{
			++sec;
			total_qps += qps_cnt;
			printf("thread [%d] ---> qps = [%ld], average = [%ld]\n", id->pid, qps_cnt, total_qps / sec);
			qps_cnt = 0;
			last_time = curr_time;
		}
	}

	return nullptr;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		usage();
		exit(1);
	}
	int cnt = atoi(argv[1]);
	pthread_t* ths = new pthread_t[cnt];
	ID* ids = new ID[cnt];
	for (int i = 0; i < cnt; i++)
	{
		ids[i].pid = i;
		ids[i].modid = i % 5 + 1;
		ids[i].cmdid = 1;
		if (pthread_create(&ths[i], nullptr, qps_main, &ids[i]) < 0)
		{
			fprintf(stderr, "create pthread error\n");
			exit(1);
		}
	}
	for (int i = 0; i < cnt; i++) {
		pthread_join(ths[i], NULL);
	}
	delete[]ids;
	delete[]ths;

	return 0;
}