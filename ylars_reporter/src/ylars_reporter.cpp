#include "store_report.h"

static int db_thread_cnt;
static store_queue_t** task_queues;

void deal_report(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	// 解析请求数据
	ylars::ReportStatusReq req;
	if (!req.ParseFromArray(msg, msglen))
	{
		return;
	}

	// 单线程工作
	//StoreReport srt;
	//srt.report2db(req);

	// 多线程工作
	static int index = 0;
	task_queues[index++]->send_task(req);
	index %= db_thread_cnt;
}

// 将网络读取和数据库io操作分离，用线程池进行数据库io操作
void create_reportdb_threads(void)
{
	// 读配置文件获取线程个数
	db_thread_cnt = config_file::instance()->
		GetNumber("reporter", "db_thread_cnt", 3);

	// 创建处理队列
	task_queues = new store_queue_t * [db_thread_cnt];
	if (nullptr == task_queues)
	{
		// 任务队列创建失败
		fprintf(stderr, "create task queues error\n");
		exit(1);
	}
	for (int i = 0; i < db_thread_cnt; i++)
	{
		pthread_t tid;
		task_queues[i] = new store_queue_t{};
		if (nullptr == task_queues[i])
		{
			fprintf(stderr, "create %d task queue error\n", i);
			exit(1);
		}

		if (pthread_create(&tid, nullptr, store_main, task_queues[i]) < 0)
		{
			fprintf(stderr, "create %d thread error\n", i);
			exit(1);
		}
		// 线程分离
		pthread_detach(tid);
	}
}

int main(int agrc, char** argv)
{
	// 加载配置文件
	if (!config_file::setPath("./conf/ylars_reporter.ini"))
	{
		fprintf(stderr, "./conf/ylars_agent.ini file not found\n");
		exit(1);
	}
	auto conf = config_file::instance();
	// 获取配置的ip和端口号
	std::string ip = conf->GetString("reactor", "ip", "0.0.0.0");
	uint16_t port = (uint16_t)conf->GetNumber("reactor", "port", 7779);

	// 创建事件循环器
	ylars::event_loop loop;
	// 初始化服务器
	ylars::tcp_server ser{ &loop, ip.c_str(), port };

	ser.register_msgid(ylars::ID_ReportStatusRequest, deal_report);

	create_reportdb_threads();

	loop.start_process();
}

