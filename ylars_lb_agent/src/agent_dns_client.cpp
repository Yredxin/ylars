#include "agent_main.h"

std::unique_ptr<dns_queue_t>dns_queue = nullptr;

void dns_task_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto dns_cli = static_cast<ylars::tcp_client*>(args);
	assert(dns_cli != nullptr);
	// 获取队列中的所有任务
	std::queue<ylars::GetRouteRequest> tasks;
	dns_queue->recv_tasks(tasks);

	// 发送请求给服务器
	ylars::GetRouteRequest task;
	std::string req_str;
	while (!tasks.empty())
	{
		// 取出头节点
		task = std::move(tasks.front());
		tasks.pop();

		task.SerializeToString(&req_str);
		dns_cli->send_msg(ylars::ID_GetRouteRequest, req_str.c_str(), (int)req_str.length());
	}
}

void dns_res_cb(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	// 服务器应答
	ylars::GetRouteResponse res;
	if (!res.ParseFromArray(msg, msglen))
	{
		return;
	}

	// 将数据添加到route lb
	uint32_t modid, cmdid;
	modid = res.modid();
	cmdid = res.cmdid();
	agent_routes[(modid + cmdid) % 3]->update_host(res);
}

void conn2_dns_ser(ylars::conn_base* conn, void* args)
{
	auto loop = static_cast<ylars::event_loop*>(args);
	// 等待队列中的消息
	dns_queue->set_event_loop(loop);

	// 注册任务回调
	dns_queue->set_task_cb(dns_task_cb, conn);

}

void* dns_client(void* args)
{
	assert(dns_queue != nullptr);

	// 读取配置文件
	std::string ip = config_file::instance()->GetString("dns_server", "dns_ip", "0.0.0.0");
	uint16_t port = (uint16_t)config_file::instance()->GetNumber("dns_server", "dns_port", 7778);
	ylars::event_loop loop;

	// 连接服务器
	ylars::tcp_client dns_cli{ &loop, ip.c_str(), port };

	// 注册dns server应答回调
	dns_cli.register_msgid(ylars::ID_GetRouteResponse, dns_res_cb);

	// 注册连接HOOK，连接成功添加任务队列
	dns_cli.register_conn_start(conn2_dns_ser, &loop);

	loop.start_process();
	return nullptr;
}

void start_dns_client()
{
	assert(dns_queue == nullptr);
	// 初始化dns client线程任务队列
	dns_queue.reset(new dns_queue_t);
	if (nullptr == dns_queue)
	{
		fprintf(stderr, "new dns task queue error\n");
		exit(1);
	}
	pthread_t pid;
	if (pthread_create(&pid, nullptr, dns_client, nullptr) < 0)
	{
		fprintf(stderr, "create dns client thread error\n");
		exit(1);
	}
	pthread_detach(pid);
}

