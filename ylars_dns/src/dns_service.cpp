#include <mysql/mysql.h>
#include <ylars.pb.h>
#include <ylars_reactor.h>
#include <memory>
#include <set>
#include <pthread.h>
#include <subscribe.h>
#include "dns_router.h"

std::unique_ptr<ylars::tcp_server> ser = nullptr;

typedef std::set<uint64_t> conn_mod_set_t;

void create_subscribe(ylars::conn_base* conn, void* agrs)
{
	// 给每个连接单独绑定一个模块集合，确保模块不重复
	conn->param = static_cast<void*>(new conn_mod_set_t{});
	if (nullptr == conn->param)
	{
		std::cerr << "conn mod set create error !" << std::endl;
	}
}

void clean_subscribe(ylars::conn_base* conn, void* agrs)
{
	auto mod_set = static_cast<conn_mod_set_t*>(conn->param);

	auto sub = SubscribeList::instance();
	int fd = conn->get_fd();

	for (auto& mod : *mod_set)
	{
		sub->unsubscribe(mod, fd);
	}

	if (nullptr != mod_set)
	{
		delete mod_set;
	}
	conn->param = nullptr;
}

void get_req(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	// 获取请求内容
	ylars::GetRouteRequest req;
	req.ParseFromArray(msg, msglen);

	// 获取id
	uint32_t modId, cmdId;
	modId = req.modid();
	cmdId = req.cmdid();

	// 判断是否是第一次收到该模块消息，如果是加入模块集合
	uint64_t mod = ((uint64_t)modId << 32) + cmdId;
	auto mod_set = static_cast<conn_mod_set_t*>(conn->param);
	if (nullptr != mod_set && mod_set->find(mod) == mod_set->end())
	{
		mod_set->insert(mod);
		// 订阅
		SubscribeList::instance()->subscribe(mod, conn->get_fd());
	}

	// 组织应答内容
	ylars::GetRouteResponse res;
	res.set_modid(modId);
	res.set_cmdid(cmdId);

	// 获取主机信息集合
	auto hosts = Router::instance()->get_hosts(modId, cmdId);
	ylars::HostInfo host_info;
	for (auto& host : hosts)
	{
		host_info.Clear();
		host_info.set_ip((uint32_t)(host >> 32));
		host_info.set_port((uint32_t)host);
		res.add_host()->CopyFrom(host_info);
	}

	// 应答
	std::string res_str;
	if (res.SerializeToString(&res_str))
	{
		conn->send_msg(ylars::ID_GetRouteResponse, res_str.c_str(), (int)res_str.length());
	}
	else
	{
		conn->send_msg(ylars::ID_GetRouteResponse, "error", 6);
	}
}

int main(int agrc, char** argv)
{
	// 加载配置文件
	if (!config_file::setPath("./conf/ylars_dns.ini"))
	{
		fprintf(stderr, "./conf/ylars_agent.ini file not found\n");
		exit(1);
	}
	auto conf = config_file::instance();
	// 获取配置的ip和端口号
	std::string ip = conf->GetString("reactor", "ip", "0.0.0.0");
	uint16_t port = (uint16_t)conf->GetNumber("reactor", "port", 7777);

	// 创建事件循环器
	ylars::event_loop loop;
	// 初始化服务器
	ser.reset(new ylars::tcp_server{ &loop, ip.c_str(), port });

	// 给服务器添加启动和结束hook
	ser->register_conn_start(create_subscribe);
	ser->register_conn_end(clean_subscribe);

	ser->register_msgid(ylars::ID_GetRouteRequest, get_req);

	// 创建线程测试
	// pthread_create(&test_thread, nullptr, publish_change_test, nullptr);
	// 创建推送线程
	pthread_t th_publish;
	if (pthread_create(&th_publish, nullptr, changet_publish, nullptr) < 0)
	{
		fprintf(stderr, "create publish_thread_main error \n");
		exit(1);
	}
	pthread_detach(th_publish);

	loop.start_process();
}

