#include <tcp_server.h>
#include <event_loop.h>
#include <buf_pool.h>
#include <message.h>
#include <tcp_conn.h>
#include <iostream>
#include <cstring>
#include <config_file.h>
#include <thread_pool.h>

static ylars::thread_pool* th_pool = nullptr;

void echo(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto ser = dynamic_cast<ylars::tcp_conn*>(conn);
	std::cout << "echo to client !" << std::endl;
	ser->send_msg(2, msg, msglen);
}

void show(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto ser = dynamic_cast<ylars::tcp_conn*>(conn);
	char* buf = new char[msglen + 1];
	bzero(buf, msglen + 1);
	memcpy(buf, msg, msglen);
	std::cout << "msgid : " << msgid << std::endl
		<< "msglen : " << msglen << std::endl
		<< "msg : " << buf << std::endl;
	delete[] buf;
	ser->send_msg(1, "hello, i am server !", 21);
}

void async_task(ylars::event_loop* loop, void* args)
{
	std::cout << "============= Active Task Func =============" << std::endl;
	ylars::evn_set_t fds;
	loop->get_listen_fds(fds);
	// 获取所有的连接
	auto& conns = ylars::tcp_server::conns;
	for (auto& fd : fds)
	{
		if (conns.find(fd) == conns.end())
		{
			continue;
		}
		// 获取当前线程拥有的连接
		conns[fd]->send_msg(5, "new client connection ! ", 25);
	}
}

void start(ylars::conn_base* conn, void* args)
{
	th_pool->add_async_task(async_task);
}

void qps_test(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto ser = dynamic_cast<ylars::tcp_conn*>(conn);
	ser->send_msg(3, msg, msglen);
}


int main()
{
	// 加载配置文件
	config_file::setPath("./conf/reactor.ini");
	auto conf = config_file::instance();
	auto ip = conf->GetString("reactor", "ip", "0.0.0.0");
	uint16_t port = conf->GetNumber("reactor", "port", 7777);
	ylars::event_loop loop;
	ylars::tcp_server tcp{ &loop, ip.c_str(), port };
	th_pool = tcp.get_thread_pool();
	// tcp.register_conn_start(start);
	// tcp.register_msgid(1, echo);
	// tcp.register_msgid(2, show);
	tcp.register_msgid(3, qps_test);
	loop.start_process();
	return 0;
}