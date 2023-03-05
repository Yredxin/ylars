#include "agent_main.h"

std::vector<std::unique_ptr<route_lb>> agent_routes;

// 获取当前节点全部主机
void route_req_cb(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	long index = (long)args;
	ylars::GetRouteRequest req;
	if (!req.ParseFromArray(msg, msglen))
	{
		return;
	}
	uint32_t modid, cmdid;
	modid = req.modid();
	cmdid = req.cmdid();

	ylars::GetRouteResponse res;
	res.set_modid(modid);
	res.set_cmdid(cmdid);
	// 获取主机集
	agent_routes[index]->get_route(modid, cmdid, res);

	// 发送数据
	std::string res_str;
	res.SerializeToString(&res_str);
	conn->send_msg(ylars::ID_GetRouteResponse, res_str.c_str(), (int)res_str.length());
}

// 获取单个主机
void host_req_cb(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	long index = (long)args;

	// 解析客户端请求
	ylars::GetHostRequest req;
	if (!req.ParseFromArray(msg, msglen))
	{
		return;
	}

	uint32_t modid, cmdid;
	modid = req.modid();
	cmdid = req.cmdid();

	ylars::GetHostResponse res;
	res.set_modid(modid);
	res.set_cmdid(cmdid);
	res.set_seq(req.seq());

	// 获取主机
	agent_routes[index]->get_host(modid, cmdid, res);

	// 发送数据
	std::string res_str;
	res.SerializeToString(&res_str);
	conn->send_msg(ylars::ID_GetHostResponse, res_str.c_str(), (int)res_str.length());
}

// 上报主机
void report_req_cb(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	long index = (long)args;
	ylars::ReportRequest req;
	req.ParseFromArray(msg, msglen);
	agent_routes[index]->report_host(req);
}

void* agent_udp_server(void* args)
{
	// assert(agent_routes != nullptr);
	long index = (long)args;

	ylars::event_loop loop;


	// 创建udp对应的route lb
	agent_routes[index].reset(new route_lb);
	if (nullptr == agent_routes[index])
	{
		fprintf(stderr, "create agent routes error\n");
		exit(1);
	}

	// 三个udp分别为 9995 9996 9997
	ylars::udp_server agent_ser{ &loop, "127.0.0.1", (uint16_t)(9995 + index) };

	// 注册应答模块
	// udp请求应答
	agent_ser.register_msgid(ylars::ID_GetHostRequest, host_req_cb, args);

	agent_ser.register_msgid(ylars::ID_GetRouteRequest, route_req_cb, args);

	// report请求
	agent_ser.register_msgid(ylars::ID_ReportRequest, report_req_cb, args);

	std::cout << "start udp server succ, port is " << 9995 + index << std::endl;

	loop.start_process();
	return nullptr;
}

void start_udp_server()
{
	// 默认主机个数为3
	// 创建3个线程
	pthread_t ths[3];
	for (long i = 0; i < 3; i++)
	{
		agent_routes.emplace_back();
		if (pthread_create(&ths[i], nullptr, agent_udp_server, (void*)i) < 0)
		{
			fprintf(stderr, "create %ld agent udp server error\n", i);
			exit(1);
		}
		pthread_detach(ths[i]);
	}

}