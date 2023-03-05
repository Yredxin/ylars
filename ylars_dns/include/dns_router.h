#ifndef __YLARS_DNS_ROUTER__
#define __YLARS_DNS_ROUTER__
#include <pthread.h>
#include <mysql/mysql.h>
#include <map>
#include <set>
#include <vector>
#include <stdint.h>

// 数据记录表
#define ROURE_DATA_TABLE "RouteData"

/* dns路由，用于获取数据库内容，往上推送 */
class Router
{
public:
	typedef std::set<uint64_t> ip_port_t;
	typedef std::map<uint64_t, ip_port_t> router_map_data_t;


	// 获取单例对象
	static Router* instance();

	// 获取指定模块的所有主机
	ip_port_t get_hosts(int modId, int cmdId);

	// 加载mod表
	inline void load_route_data()
	{
		build_map();
	}

	// 判断版本号是否改变
	int check_version();

	// 加载改变的模块数据
	void load_change_mod(std::vector<uint64_t>& mods);

	// 交换数据表
	void swap();

	// 删除同步的数据
	void drop_synchronous_mod();

private:

	/*========== 单例的实现 ==========*/
	Router();
	Router(const Router&) = delete;
	Router& operator=(const Router&) = delete;
	~Router();

	// 用于全局唯一一次初始化
	static void init();

	// 单例对象
	static Router* _router;

	// 保证全局唯一单例锁
	static pthread_once_t _once;

	/*========== 具体业务 ==========*/
	// 数据库对象
	MYSQL _conn_db;

	// 连接数据库
	void connection2db();

	// 用于读写的路由表，存放数据库内容
	router_map_data_t* _data_pointer; // 指向RouterDataMap_A 当前的关系map
	router_map_data_t* _temp_pointer; // 指向RouterDataMap_B 当前的关系map

	// 读写锁
	pthread_rwlock_t _rwlock;

	// sql语句暂存位置
	char _sql[1024];

	// 当前版本信息
	long _version;

	// 初始化map表
	void build_map(bool is_init = false);

};

#endif // !__YLARS_DNS_ROUTER__
