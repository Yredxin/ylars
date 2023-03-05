#ifndef __YLARS_LOAD_BALANCE__
#define __YLARS_LOAD_BALANCE__
#include <map>
#include <list>
#include <ylars.pb.h>
#include "host_info.h"

typedef std::map<uint64_t, std::shared_ptr<host_info>> host_map_t;
typedef std::list<std::shared_ptr<host_info>> host_list_t;

class load_balance
{
public:
	load_balance(uint32_t modid, uint32_t cmdid) :
		_modid{ modid },
		_cmdid{ cmdid },
		_access_cnt{ 0 }
	{

	}

	// 是否存在主机信息
	bool empty() const
	{
		return _host_map.empty();
	}

	// 拉取所有主机
	void pull();
	// 更新表数据
	void update(ylars::GetRouteResponse& res);
	// 获取一个主机
	int choice_one_host(ylars::GetHostResponse& res);
	// 负载均衡实现，overload<==>idle相互过渡
	void report(uint32_t ip, uint32_t port, bool is_succ);
	// 上传到report服务器
	void commit();
	// 获取全部主机
	int get_all_host(std::vector<std::shared_ptr<host_info>>& hosts);
	enum STATUS
	{
		PULLING,	// 拉取中
		NEW			// 存在数据
	} status;		// 当前状态

	// 定期更新列表
	long last_update_ts;
private:
	uint32_t _modid;
	uint32_t _cmdid;

	// 存储主机信息
	host_map_t _host_map;

	// 空闲队列
	host_list_t _idle_list;
	// 过载队列
	host_list_t _overload_list;

	// 准备访问负载列表的进度
	int _access_cnt;

	// 头部取出主机，之后移至尾部
	static void	get_host_from_list(ylars::GetHostResponse& res, host_list_t& host_l);
};

#endif // !__YLARS_LOAD_BALANCE__
