#include "dns_router.h"
#include <cstring>
#include <cstdio>
#include <stdlib.h>
#include <ylars_reactor.h>

Router* Router::_router = nullptr;
pthread_once_t Router::_once = PTHREAD_ONCE_INIT;

void Router::init()
{
	_router = new Router{};
	if (nullptr == _router)
	{
		fprintf(stderr, "new Router error \n");
		exit(1);
	}
	fprintf(stdout, "create Router\n");
}

void Router::connection2db()
{
	// 获取数据库信息
	auto conf = config_file::instance();
	auto ip = conf->GetString("mysql", "db_ip", "0.0.0.0");
	auto port = conf->GetNumber("mysql", "db_port", 3306);
	auto user = conf->GetString("mysql", "db_user", "root");
	auto passwd = conf->GetString("mysql", "db_passwd", "");
	auto db_name = conf->GetString("mysql", "db_name", "ylars_dns");

	// 初始化数据库句柄
	if (!mysql_init(&_conn_db))
	{
		printf("mysql init error, In case of insufficient memory");
		exit(1);
	}

	// 设置数据库超时定期重连，每30秒重连一次
	mysql_options(&_conn_db, MYSQL_OPT_CONNECT_TIMEOUT, "30");
	// 开启数据库定时重连
	int reconnect = 1;
	mysql_options(&_conn_db, MYSQL_OPT_RECONNECT, &reconnect);

	// 连接数据库
	if (!mysql_real_connect(&_conn_db, ip.c_str(), user.c_str(),
		passwd.c_str(), db_name.c_str(), port, nullptr, 0))
	{
		fprintf(stderr, "mysql connect error, error is %s", mysql_error(&_conn_db));
		exit(1);
	}
	printf("mysql connect succ !\n");
}

void Router::build_map(bool is_init)
{
	auto pointer_map = is_init ? _data_pointer : _temp_pointer;
	pointer_map->clear();

	snprintf(_sql, sizeof(_sql), "select * from %s", ROURE_DATA_TABLE);

	// 查询数据库
	if (mysql_real_query(&_conn_db, _sql, strlen(_sql)) != 0)
	{
		fprintf(stderr, "mysql query error, error is %s\n", mysql_error(&_conn_db));
		return;
	}

	// 遍历结果集
	MYSQL_RES* result = mysql_store_result(&_conn_db);
	if (nullptr == result)
	{
		printf("No results \n");
		return;
	}

	auto cols = mysql_num_fields(result);
	if (cols >= 5)
	{
		// 加写锁
		pthread_rwlock_wrlock(&_rwlock);
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result)))
		{
			int modId = atoi(row[1]);
			int cmdId = atoi(row[2]);
			unsigned int ip = atoi(row[3]);
			int port = atoi(row[4]);
			uint64_t key = ((uint64_t)modId << 32) + cmdId;
			uint64_t value = ((uint64_t)ip << 32) + port;
			(*pointer_map)[key].insert(value);
		}
		pthread_rwlock_unlock(&_rwlock);
	}

	mysql_free_result(result);
}

Router* Router::instance()
{
	pthread_once(&_once, init);
	return _router;
}

Router::ip_port_t Router::get_hosts(int modId, int cmdId)
{
	uint64_t key = ((uint64_t)modId << 32) + cmdId;
	pthread_rwlock_rdlock(&_rwlock);
	if (_data_pointer->find(key) == _data_pointer->end())
	{
		pthread_rwlock_unlock(&_rwlock);
		return ip_port_t();
	}
	auto ret = (*_data_pointer)[key];
	pthread_rwlock_unlock(&_rwlock);
	return ret;
}

Router::Router() :_version{ 0 }
{
	// 连接数据库
	connection2db();

	// 初始化读写锁
	pthread_rwlock_init(&_rwlock, nullptr);

	// 创建向量表
	_data_pointer = new router_map_data_t{};
	if (nullptr == _data_pointer)
	{
		fprintf(stderr, "new map data pointer error !\n");
		exit(1);
	}
	_temp_pointer = new router_map_data_t{};
	if (nullptr == _temp_pointer)
	{
		fprintf(stderr, "new temp map data pointer error !\n");
		exit(1);
	}

	// 初始化向量表
	build_map(true);
}

Router::~Router()
{
	mysql_close(&_conn_db);
	delete _data_pointer;
	delete _temp_pointer;
}

// 查询数据库版本号
int Router::check_version()
{
	memset(_sql, 0, sizeof(_sql));
	sprintf(_sql, "select version from RouteVersion where id = 1");
	// 查询数据库
	if (mysql_real_query(&_conn_db, _sql, strlen(_sql)) != 0)
	{
		fprintf(stderr, "mysql query error, error is %s\n", mysql_error(&_conn_db));
		return -1;
	}

	// 获取结果集
	auto result = mysql_store_result(&_conn_db);
	if (nullptr == result)
	{
		// 查询出错
		printf("No results \n");
		return -1;
	}

	int ret = 0;
	// 获取版本号
	if (mysql_num_rows(result) > 0)
	{
		auto new_version = atoi(mysql_fetch_row(result)[0]);
		if (new_version > this->_version)
		{
			this->_version = new_version;
			ret = 1;
		}
	}

	// 释放结果集
	mysql_free_result(result);
	return ret;
}

// 加载改变的route
void Router::load_change_mod(std::vector<uint64_t>& mods)
{
	memset(_sql, 0, sizeof(_sql));
	sprintf(_sql, "select modid, cmdid from RouteChange where version <= %ld", this->_version);
	// 查询数据库
	if (mysql_real_query(&_conn_db, _sql, strlen(_sql)) != 0)
	{
		fprintf(stderr, "mysql query error, error is %s\n", mysql_error(&_conn_db));
		return;
	}
	// 获取结果集
	auto result = mysql_store_result(&_conn_db);
	if (nullptr == result)
	{
		// 查询出错
		printf("No results \n");
		return;
	}
	MYSQL_ROW row;
	uint32_t modid, cmdid;
	while ((row = mysql_fetch_row(result)))
	{
		// 查询到结果
		modid = atoi(row[0]);
		cmdid = atoi(row[1]);
		mods.push_back(((uint64_t)modid << 32) + cmdid);
	}

	// 加载查看数据是否已被删除，原表已更新在_temp_pointer中
	// _temp_pointer中存在且_data_pointer中不存在的，即为已删除数据
	for (auto& temp : *_temp_pointer)
	{
		if (_data_pointer->find(temp.first) == _data_pointer->end())
		{
			// 此模块已删除
			mods.push_back(temp.first);
			continue;
		}
		for (auto& ip_port : temp.second)
		{
			if ((*_data_pointer)[temp.first].find(ip_port) ==
				(*_data_pointer)[temp.first].end())
			{
				// 此主机已删除
				mods.push_back(temp.first);
				break;
			}
		}
	}
}

void Router::swap()
{
	pthread_rwlock_wrlock(&_rwlock);
	auto temp = this->_temp_pointer;
	this->_temp_pointer = this->_data_pointer;
	this->_data_pointer = temp;
	pthread_rwlock_unlock(&_rwlock);
}

void Router::drop_synchronous_mod()
{
	memset(_sql, 0, sizeof(_sql));
	sprintf(_sql, "delete from RouteChange where version <= %ld", this->_version);
	// 执行删除语句
	if (mysql_real_query(&_conn_db, _sql, strlen(_sql)) != 0)
	{
		fprintf(stderr, "mysql query error, error is %s\n", mysql_error(&_conn_db));
		return;
	}
}

