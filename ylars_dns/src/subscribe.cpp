#include "subscribe.h"
#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <memory>
#include <ylars.pb.h>
#include <dns_router.h>
#include <tcp_conn.h>

extern std::unique_ptr<ylars::tcp_server> ser;

SubscribeList* SubscribeList::_subscribe = nullptr;
pthread_once_t SubscribeList::_once = PTHREAD_ONCE_INIT;

// 发布任务
void publish_change_task(ylars::event_loop* loop, void* args)
{
	auto sub = static_cast<SubscribeList*>(args);
	if (nullptr == sub)
	{
		return;
	}
	// 获取当前线程集合
	ylars::evn_set_t fds;
	loop->get_listen_fds(fds);
	// 获取当前线程的任务集合
	pub_map_t need_publish;
	sub->make_publish_map(fds, need_publish);

	ylars::GetRouteResponse res{};
	ylars::HostInfo host_info{};
	uint32_t modid, cmdid;
	std::string res_str;
	// 制作应答消息
	for (auto& it : need_publish)
	{
		for (auto& mod : it.second)
		{
			modid = (uint32_t)(mod >> 32);
			cmdid = (uint32_t)mod;
			res.set_modid(modid);
			res.set_cmdid(cmdid);
			auto ip_ports = Router::instance()->get_hosts(modid, cmdid);
			for (auto& ip_port : ip_ports)
			{
				host_info.set_ip((uint32_t)(ip_port >> 32));
				host_info.set_port((uint32_t)ip_port);
				res.add_host()->CopyFrom(host_info);
				host_info.Clear();
			}

			// 发布消息
			res.SerializeToString(&res_str);
			auto conn = ser->conns[it.first];
			if (nullptr != conn)
			{
				conn->send_msg(ylars::ID_GetRouteResponse, res_str.c_str(), (int)res_str.length());
			}
			res.Clear();
		}
	}
}

SubscribeList* SubscribeList::instance()
{
	pthread_once(&_once, init);
	return _subscribe;
}

// 订阅
void SubscribeList::subscribe(uint64_t mod, int fd)
{
	pthread_mutex_lock(&_book_mutex);
	_book_list[mod].insert(fd);
	pthread_mutex_unlock(&_book_mutex);
}

// 取消订阅
void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
	pthread_mutex_lock(&_book_mutex);
	if (_book_list.find(mod) != _book_list.end())
	{
		// 清除模块对应集合中的fd
		_book_list[mod].erase(fd);
		// 如果模块已没有fd则清除模块
		if (_book_list[mod].empty())
		{
			_book_list.erase(mod);
		}
	}
	pthread_mutex_unlock(&_book_mutex);
}

// 发布
void SubscribeList::publish(std::vector<uint64_t>& change_mods)
{
	pthread_mutex_lock(&_book_mutex);
	pthread_mutex_lock(&_push_mutex);
	for (auto& mod : change_mods)
	{
		if (_book_list.find(mod) == _book_list.end())
		{
			continue;
		}
		for (auto& fd : _book_list[mod])
		{
			_push_list[fd].insert(mod);
		}
	}
	pthread_mutex_unlock(&_push_mutex);
	pthread_mutex_unlock(&_book_mutex);
	// 发布异步任务
	ser->get_thread_pool()->add_async_task(publish_change_task, this);
}

// 制作一个线程需要的任务集合
void SubscribeList::make_publish_map(ylars::evn_set_t& fds, pub_map_t& need_publish)
{
	// 合并文件描述符集合，筛选需要的任务放入need_publish
	pthread_mutex_lock(&_push_mutex);
	for (auto& fd : fds)
	{
		if (_push_list.find(fd) == _push_list.end())
		{
			continue;
		}
		_push_list[fd].swap(need_publish[fd]);
		_push_list.erase(fd);
	}
	pthread_mutex_unlock(&_push_mutex);
}

SubscribeList::SubscribeList()
{
	pthread_mutex_init(&_book_mutex, nullptr);
	pthread_mutex_init(&_push_mutex, nullptr);
}

SubscribeList::~SubscribeList()
{
	pthread_mutex_destroy(&_book_mutex);
	pthread_mutex_destroy(&_push_mutex);
}

void SubscribeList::init()
{
	_subscribe = new SubscribeList{};
	if (nullptr == _subscribe)
	{
		fprintf(stderr, "new subscribelist error \n");
		exit(1);
	}
}

void* publish_change_test(void* args)
{
	auto sub = SubscribeList::instance();
	std::vector<uint64_t> mods;
	while (true)
	{
		mods.clear();
		sleep(1);
		uint32_t modId1 = 1;
		uint32_t cmdId1 = 1;
		mods.push_back(((uint64_t)modId1 << 32) + cmdId1);

		uint32_t modId2 = 1;
		uint32_t cmdId2 = 2;
		mods.push_back(((uint64_t)modId2 << 32) + cmdId2);
		sub->publish(mods);
	}
	return nullptr;
}

// 主动推送任务
void* changet_publish(void* args)
{
	// 每个1秒查询一次，每个10秒强制更新
	time_t update_sec = 10;
	time_t start_time = time(nullptr);
	time_t curr_time = start_time;
	auto router = Router::instance();
	std::vector<uint64_t> mods;
	while (true)
	{
		sleep(1);
		// 获取当前时间
		curr_time = time(nullptr);
		// 查询版本是否更新
		int ret = router->check_version();
		if (ret > 0)
		{
			mods.clear();
			// 数据发生更新
			// 1. 更新数据表
			router->load_route_data();
			router->swap();
			// 2. 获取改变的数据
			router->load_change_mod(mods);
			// 3. 推送数据
			SubscribeList::instance()->publish(mods);
			// 4. 删除已同步的数据
			router->drop_synchronous_mod();
		}
		else if (ret == 0)
		{
			// 未发生更新
			if (curr_time - start_time > update_sec)
			{
				// 超过10秒强制更新表格
				router->load_route_data();
				router->swap();
			}
		}
		else
		{
			// 查询出错
			break;
		}
	}
	return nullptr;
}
