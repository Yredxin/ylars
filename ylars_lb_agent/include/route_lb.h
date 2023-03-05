#ifndef __YLARS_ROUTE_LB__
#define __YLARS_ROUTE_LB__
#include "load_balance.h"
#include "ylars.pb.h"
#include <memory>
#include <pthread.h>

typedef std::map<uint64_t, std::unique_ptr<load_balance>> route_map_t;

class route_lb
{
public:
	route_lb();
	~route_lb();
	// 获取一个主机
	int get_host(uint32_t modid, uint32_t cmdid, ylars::GetHostResponse& res);
	// 获取当前模块所有主机
	int get_route(uint32_t modid, uint32_t cmdid, ylars::GetRouteResponse& res);
	// 上报主机信息
	void report_host(ylars::ReportRequest& req);

	// 更新数据
	void update_host(ylars::GetRouteResponse& res);
private:
	route_map_t _route_map;
	pthread_mutex_t _route_mutex;
};

#endif // !__YLARS_ROUTE_LB__
