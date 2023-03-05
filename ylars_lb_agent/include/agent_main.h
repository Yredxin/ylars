#ifndef __YLARS_AGENT_MAIN__
#define __YLARS_AGENT_MAIN__
#include <ylars_reactor.h>
#include <ylars.pb.h>
#include <pthread.h>
#include <memory>
#include "route_lb.h"

// 启动一个dns客户端线程
void start_dns_client();

// 启动一个report客户端线程
void start_report_client();

// 启动三个对外udp服务线程
void start_udp_server();

// dns任务队列
typedef ylars::thread_queue<ylars::GetRouteRequest> dns_queue_t;
extern std::unique_ptr<dns_queue_t>dns_queue;

// report任务队列
typedef ylars::thread_queue<ylars::ReportStatusReq> report_queue_t;
extern std::unique_ptr<report_queue_t>report_queue;

// 一个udp对应一个route lb
extern std::vector<std::unique_ptr<route_lb>> agent_routes;

struct load_balance_config
{
	// 若干次请求，访问一次负载的阈值
	int	probe_num;
	// 初始虚拟成功次数，防止少量失败过载
	int init_succ_cnt;
	// 初始失败次数，防止少量成功转换
	int init_err_cnt;

	// 切换为空闲需要的成功率
	float succ_rate;
	// 切换为负载的失败率
	float err_rate;

	// 连续成功阈值
	int	contin_succ_limit;
	// 连续失败阈值
	int contin_err_limit;

	//对于某个modid/cmdid下的idle状态的主机，需要清理一次负载窗口的时间
	int idle_timeout;

	//对于某个modid/cmdid下的overload状态主机， 在过载队列等待的最大时间
	int overload_timeout;

	//对于每个NEW状态的modid/cmdid 多久从远程dns更新一次到本地路由
	long update_timeout;
};

extern load_balance_config lb_config;

#endif // !__YLARS_AGENT_MAIN__
