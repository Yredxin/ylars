#include "agent_main.h"

load_balance_config lb_config;

void init_agent_from_config()
{
	// 初始化配置文件
	if (!config_file::setPath("./conf/ylars_agent.ini"))
	{
		fprintf(stderr, "./conf/ylars_agent.ini file not found\n");
		exit(1);
	}

	auto conf = config_file::instance();
	// 初始化load_balance阈值次数
	lb_config.probe_num = conf->GetNumber("load_balance", "probe_num", 10);
	// 初始虚拟成功次数，防止少量失败过载
	lb_config.init_succ_cnt = conf->GetNumber("load_balance", "init_succ_cnt", 180);
	// 初始失败次数，防止少量成功转换
	lb_config.init_err_cnt = conf->GetNumber("load_balance", "init_err_cnt", 5);

	// 切换为空闲需要的成功率
	lb_config.succ_rate = conf->GetFloat("load_balance", "succ_rate", 0.95);
	// 切换为负载的失败率
	lb_config.err_rate = conf->GetFloat("load_balance", "err_rate", 0.1);

	// 连续成功阈值
	lb_config.contin_succ_limit = conf->GetNumber("load_balance", "contin_succ_limit", 10);
	// 连续失败阈值
	lb_config.contin_err_limit = conf->GetNumber("load_balance", "contin_err_limit", 10);
	//对于某个modid/cmdid下的idle状态的主机，需要清理一次负载窗口的时间
	lb_config.idle_timeout = conf->GetNumber("load_balance", "idle_timeout", 15);

	//对于某个modid/cmdid下的overload状态主机， 在过载队列等待的最大时间
	lb_config.overload_timeout = conf->GetNumber("load_balance", "overload_timeout", 15);

	//对于每个NEW状态的modid/cmdid 多久从远程dns更新一次到本地路由
	lb_config.update_timeout = conf->GetNumber("load_balance", "update_timeout", 15);
}

int main()
{
	init_agent_from_config();

	// 一个dns客户端线程
	start_dns_client();

	// 一个report客户端线程
	start_report_client();

	// 三个对外udp服务线程
	start_udp_server();

	// 保证主线程不退出
	while (true)
	{
		sleep(10);
	}

	return 0;
}