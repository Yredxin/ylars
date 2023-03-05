#include "load_balance.h"
#include "agent_main.h"

void load_balance::pull()
{
	assert(dns_queue != nullptr);

	// 组织dns请求消息
	ylars::GetRouteRequest req;
	req.set_modid(_modid);
	req.set_cmdid(_cmdid);

	// 往队列中添加数据
	dns_queue->send_task(req);

	// 修改状态
	status = PULLING;
}

void load_balance::update(ylars::GetRouteResponse& res)
{
	last_update_ts = time(nullptr);
	std::set<uint64_t> remote_hosts;	// 记录所有的远程主机
	std::set<uint64_t> need_delete;		// 将远程主机不存在的记录其中，便于删除

	// 将主机信息更新到内存
	uint64_t key;
	for (int i = 0; i < res.host_size(); i++)
	{
		auto& host = res.host(i);
		key = ((uint64_t)host.ip() << 32) + host.port();
		remote_hosts.insert(key);
		if (_host_map.find(key) != _host_map.end())
		{
			continue;
		}
		_host_map[key] = std::make_shared<host_info>(host.ip(), host.port(), lb_config.init_succ_cnt);
		if (_host_map[key] == nullptr)
		{
			_host_map.erase(key);
			fprintf(stderr, "new host info error\n");
			exit(1);
		}
		_idle_list.push_back(_host_map[key]);
	}
	// 筛选不存在的主机
	for (auto& host_it : _host_map)
	{
		if (remote_hosts.find(host_it.first) == remote_hosts.end())
		{
			need_delete.insert(host_it.first);
		}
	}
	// 删除不存在的主机
	for (auto& del_host : need_delete)
	{
		auto& host = _host_map[del_host];
		if (host->overload)
		{
			_overload_list.remove(host);
		}
		else
		{
			_idle_list.remove(host);
		}
		_host_map.erase(del_host);
	}
	status = NEW;
}

int load_balance::choice_one_host(ylars::GetHostResponse& res)
{
	if (_host_map.empty())
	{
		// 没有主机
		return ylars::RET_NOEXIST;
	}
	// 1. 如果idle_list为空，表示当前没有可用节点
	if (_idle_list.empty())
	{
		if (_access_cnt >= lb_config.probe_num)
		{
			// 如果超过阈值，则在overload_list获取
			_access_cnt = 0;
			get_host_from_list(res, _overload_list);
		}
		else
		{
			// 当前全部节点超载
			++_access_cnt;
			return ylars::RET_OVERLOAD;
		}
	}
	else
	{
		// 2. 如果overload_list为空，直接在idle_list选择
		if (_overload_list.empty())
		{
			get_host_from_list(res, _idle_list);
		}
		else if (_access_cnt >= lb_config.probe_num)
		{
			// 超过阈值，则在overload选择
			_access_cnt = 0;
			get_host_from_list(res, _overload_list);
		}
		else
		{
			++_access_cnt;
			get_host_from_list(res, _idle_list);
		}
	}
	return ylars::RET_SUCC;
}

int load_balance::get_all_host(std::vector<std::shared_ptr<host_info>>& hosts)
{
	if (_host_map.empty())
	{
		// 没有主机
		return ylars::RET_NOEXIST;
	}
	for (auto& host : _host_map)
	{
		hosts.push_back(host.second);
	}
	return ylars::RET_SUCC;
}

void load_balance::report(uint32_t ip, uint32_t port, bool is_succ)
{
	// 记录当前时间
	long curr_time = time(nullptr);
	// 获取主机
	uint64_t key = ((uint64_t)ip << 32) + port;

	if (_host_map.find(key) == _host_map.end())
	{
		return;
	}
	auto& host = _host_map[key];

	// 更新主机状态
	if (is_succ)
	{
		host->vsucc_cnt++;
		host->rsucc_cnt++;
		// 连续成功次数增加
		host->contin_succ++;
		// 重置连续失败次数
		host->contin_err = 0;
	}
	else
	{
		host->verr_cnt++;
		host->rerr_cnt++;
		// 连续失败次数增加
		host->contin_err++;
		// 重置连续成功次数
		host->contin_succ = 0;
	}

	// 节点状态判断
	if (!host->overload && !is_succ)		// 当前节点非负载且调用失败
	{
		bool overload = false;
		// 负载切换
		// 1. 虚拟失败率达到阈值，则切换为负载
		// 2. 连续失败次数达到阈值，则切换为负载

		// 计算失败率
		double err_rate = host->verr_cnt * 1.0 / (host->vsucc_cnt + host->verr_cnt);
		if (err_rate >= lb_config.err_rate)
		{
			// 超过失败率
			overload = true;
		}
		else if (host->contin_err >= lb_config.contin_err_limit)
		{
			// 连续失败次数
			overload = true;
		}

		if (overload)
		{
			// 切换为负载
			host->set_overload();
			// 从idle移除
			_idle_list.remove(host);
			// 加入到overload
			_overload_list.push_back(host);
		}
	}
	else if (host->overload && is_succ)		// 当前节点负载且调用成功
	{
		// 正常切换
		// 1. 虚拟成功率到达阈值，则切换为正常
		// 2. 连续成功次数达到阈值，切换为正常

		bool idle = false;
		// 负载切换
		// 1. 虚拟失败率达到阈值，则切换为负载
		// 2. 连续失败次数达到阈值，则切换为负载

		// 计算成功率
		double succ_rate = host->vsucc_cnt * 1.0 / (host->vsucc_cnt + host->verr_cnt);
		if (succ_rate >= lb_config.succ_rate)
		{
			// 超过成功率
			idle = true;
		}
		else if (host->contin_succ >= lb_config.contin_succ_limit)
		{
			// 连续成功次数
			idle = true;
		}

		if (idle)
		{
			// 切换为负载
			host->set_idle();
			// 从idle移除
			_overload_list.remove(host);
			// 加入到overload
			_idle_list.push_back(host);
		}
	}

	// 如果处于超载则定时拿下来，如果非超载则清理成功次数
	if (host->overload)
	{
		if (curr_time - host->overload_ts >= lb_config.overload_timeout)
		{
			// 在超载上超过预定时间，将其取下使用
			_overload_list.remove(host);
			_idle_list.push_back(host);
			host->set_idle();
		}
	}
	else
	{
		if (curr_time - host->idle_ts >= lb_config.idle_timeout)
		{
			// 清理成功次数，防止次数过高短时期无法判断错误主机
			host->set_idle();
		}
	}
}

void load_balance::commit()
{
	if (this->empty())
	{
		return;
	}
	ylars::ReportStatusReq req;
	req.set_modid(_modid);
	req.set_cmdid(_cmdid);
	req.set_caller(123);	// TODO 本机ip
	req.set_ts(time(nullptr));

	// 上报非负载的
	for (auto& host : _idle_list)
	{
		auto result = req.mutable_result()->Add();
		result->set_ip(host->ip);
		result->set_port(host->port);
		result->set_succ(host->rsucc_cnt);
		result->set_err(host->rerr_cnt);
		result->set_overload(false);
	}

	// 上报负载的
	for (auto& host : _overload_list)
	{
		auto result = req.mutable_result()->Add();
		result->set_ip(host->ip);
		result->set_port(host->port);
		result->set_succ(host->rsucc_cnt);
		result->set_err(host->rerr_cnt);
		result->set_overload(true);
	}
	report_queue->send_task(req);
}

void load_balance::get_host_from_list(ylars::GetHostResponse& res, host_list_t& host_l)
{
	auto host = host_l.front();

	auto res_host = res.mutable_host();
	res_host->set_ip(host->ip);
	res_host->set_port(host->port);
	// 移至尾部
	host_l.pop_front();
	host_l.push_back(host);
}
