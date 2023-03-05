#include <tcp_client.h>
#include <event_loop.h>
#include <iostream>
#include <cstring>
#include <message.h>
#include <time.h>
#include <config_file.h>
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct QPS
{
	time_t sec;
	int count;
	int n;
};

std::string ip;
uint16_t port;

void echo(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto cli = dynamic_cast<ylars::tcp_client*>(conn);
	std::cout << "echo to server!" << std::endl;
	cli->send_msg(2, msg, msglen);
}

void show(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto cli = dynamic_cast<ylars::tcp_client*>(conn);
	char* buf = new char[msglen + 1];
	bzero(buf, msglen + 1);
	memcpy(buf, msg, msglen);
	std::cout << "msgid : " << msgid << std::endl
		<< "msglen : " << msglen << std::endl
		<< "msg : " << buf << std::endl;
	delete[] buf;
	//	cli->disconnect();
}

void start(ylars::conn_base* conn, void* args)
{
	auto cli = dynamic_cast<ylars::tcp_client*>(conn);
	cli->send_msg(1, "hello, I am client!", 20);
}
void qps_test(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto cli = dynamic_cast<ylars::tcp_client*>(conn);
	auto qps = static_cast<QPS*>(args);
	qps->count++;
	if (time(nullptr) - qps->sec > 1)
	{
		qps->n++;
		pthread_mutex_lock(&mutex);
		std::cout << "thread : " << pthread_self() << " == qps: [" << qps->count << " / s] " << std::endl;
		pthread_mutex_unlock(&mutex);
		qps->count = 0;
		qps->sec = time(nullptr);
		if (qps->n >= 10)
		{
			cli->disconnect();
		}
	}
	cli->send_msg(3, msg, msglen);
}

void* thread_main(void* args)
{
	QPS qps;
	qps.sec = time(nullptr);
	qps.count = 0;
	qps.n = 0;
	ylars::event_loop loop{};
	ylars::tcp_client cli{ &loop, ip.c_str(), port };
	char buf[] = "Hi. This message is used for QPS testing!";
	cli.send_msg(3, buf, sizeof(buf));
	// cli.register_conn_start(start);
	// cli.register_msgid(1, show);
	// cli.register_msgid(2, echo);
	cli.register_msgid(3, qps_test, &qps);
	// cli.register_msgid(5, show);
	loop.start_process();
}

int main(int argc, char** argv)
{
	config_file::setPath("./conf/client.ini");
	auto conf = config_file::instance();
	ip = conf->GetString("server", "ip", "0.0.0.0");
	port = conf->GetNumber("server", "port", 7777);
	if (argc > 1)
	{
		// 多线程
		int trdn = atoi(argv[1]);
		pthread_t* trds = new pthread_t[trdn];
		for (int i = 0; i < trdn; i++)
		{
			pthread_create(&trds[i], nullptr, thread_main, nullptr);
		}
		for (int i = 0; i < trdn; i++)
		{
			pthread_join(trds[i], nullptr);
		}
		delete[] trds;
	}
	else
	{
		// 单线程
		thread_main(nullptr);
	}
	return 0;
}