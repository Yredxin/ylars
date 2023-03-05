#include "agent_main.h"

std::unique_ptr<report_queue_t>report_queue = nullptr;

void report_task_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto report_cli = static_cast<ylars::tcp_client*>(args);
	assert(report_cli != nullptr);
	// 获取队列中的所有任务
	std::queue<ylars::ReportStatusReq> tasks;
	report_queue->recv_tasks(tasks);

	// 发送请求给服务器
	ylars::ReportStatusReq task;
	std::string req_str;
	while (!tasks.empty())
	{
		// 取出头节点
		task = std::move(tasks.front());
		tasks.pop();

		task.SerializeToString(&req_str);
		report_cli->send_msg(ylars::ID_ReportStatusRequest, req_str.c_str(), (int)req_str.length());
	}
}

void conn2_report_ser(ylars::conn_base* conn, void* args)
{
	auto loop = static_cast<ylars::event_loop*>(args);
	// 等待队列中的消息
	report_queue->set_event_loop(loop);

	// 注册任务回调
	report_queue->set_task_cb(report_task_cb, conn);

}

void* report_client(void* args)
{
	assert(report_queue != nullptr);

	// 读取配置文件
	std::string ip = config_file::instance()->GetString("report_server", "report_ip", "0.0.0.0");
	uint16_t port = (uint16_t)config_file::instance()->GetNumber("report_server", "report_port", 7779);
	ylars::event_loop loop;

	// 连接服务器
	ylars::tcp_client report_cli{ &loop, ip.c_str(), port };

	// 注册连接HOOK，连接成功添加任务队列
	report_cli.register_conn_start(conn2_report_ser, &loop);

	loop.start_process();
	return nullptr;
}

void start_report_client()
{
	assert(report_queue == nullptr);
	// 初始化report client线程任务队列
	report_queue.reset(new report_queue_t);
	if (nullptr == report_queue)
	{
		fprintf(stderr, "new report task queue error\n");
		exit(1);
	}
	pthread_t pid;
	if (pthread_create(&pid, nullptr, report_client, nullptr) < 0)
	{
		fprintf(stderr, "create report client thread error\n");
		exit(1);
	}
	pthread_detach(pid);
}