#include "route_lb.h"
#include <agent_main.h>

route_lb::route_lb()
{
	pthread_mutex_init(&_route_mutex, nullptr);
}

route_lb::~route_lb()
{
	pthread_mutex_destroy(&_route_mutex);
}

int route_lb::get_host(uint32_t modid, uint32_t cmdid, ylars::GetHostResponse& res)
{
	int ret = 0;
	uint64_t mod = ((uint64_t)modid << 32) + cmdid;
	pthread_mutex_lock(&_route_mutex);
	if (_route_map.find(mod) == _route_map.end())
	{
		// 不存在，拉取
		_route_map[mod].reset(new load_balance{ modid, cmdid });
		_route_map[mod]->pull();
		ret = ylars::RET_NOEXIST;
	}
	else
	{
		auto& lb = _route_map[mod];
		// 判断是否没有列表数据
		if (lb->empty())
		{
			ret = ylars::RET_NOEXIST;
		}
		else
		{
			// 获取一个主机
			ret = lb->choice_one_host(res);
		}

		// 定期重新拉取数据
		if (lb->status == load_balance::NEW &&
			time(nullptr) - lb->last_update_ts >= lb_config.update_timeout)
		{
			lb->pull();
		}
	}
	pthread_mutex_unlock(&_route_mutex);
	res.set_retcode(ret);
	return ret;
}

int route_lb::get_route(uint32_t modid, uint32_t cmdid, ylars::GetRouteResponse& res)
{
	int ret = 0;
	uint64_t mod = ((uint64_t)modid << 32) + cmdid;
	pthread_mutex_lock(&_route_mutex);
	if (_route_map.find(mod) == _route_map.end())
	{
		// 不存在，拉取
		_route_map[mod].reset(new load_balance{ modid, cmdid });
		_route_map[mod]->pull();
		ret = ylars::RET_NOEXIST;
	}
	else
	{
		auto& lb = _route_map[mod];
		// 判断是否没有列表数据
		if (lb->empty())
		{
			ret = ylars::RET_NOEXIST;
		}
		else
		{
			// 获取全部主机
			std::vector<std::shared_ptr<host_info>>	hosts;
			ret = lb->get_all_host(hosts);

			for (auto& host_it : hosts)
			{
				auto host = res.add_host();
				host->set_ip(host_it->ip);
				host->set_port(host_it->port);
			}
		}

		// 定期重新拉取数据
		if (lb->status == load_balance::NEW &&
			time(nullptr) - lb->last_update_ts >= lb_config.update_timeout)
		{
			lb->pull();
		}
	}
	pthread_mutex_unlock(&_route_mutex);
	return ret;
}

void route_lb::report_host(ylars::ReportRequest& req)
{
	uint64_t mod = ((uint64_t)req.modid() << 32) + req.cmdid();
	pthread_mutex_lock(&_route_mutex);
	if (_route_map.find(mod) != _route_map.end())
	{
		// 主机确实存在，上报

		// 通过内部算法实现负载均衡
		_route_map[mod]->report(req.host().ip(), req.host().port(), req.issucc());
		// 上报给report服务器
		_route_map[mod]->commit();
	}
	pthread_mutex_unlock(&_route_mutex);
}

void route_lb::update_host(ylars::GetRouteResponse& res)
{
	uint32_t modid, cmdid;
	modid = res.modid();
	cmdid = res.cmdid();

	uint64_t key = ((uint64_t)modid << 32) + cmdid;
	pthread_mutex_lock(&_route_mutex);
	// mod存在
	if (_route_map.find(key) != _route_map.end())
	{
		// 没有数据，表示当前模块不存在任何主机
		if (res.host_size() == 0)
		{
			_route_map.erase(key);
		}
		else
		{
			// 更新到数据表
			_route_map[key]->update(res);
		}
	}

	pthread_mutex_unlock(&_route_mutex);
}
