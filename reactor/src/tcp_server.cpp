#include <iostream>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <cstring>
#include <netinet/tcp.h>
#include <unistd.h>
#include <ctime>
#include "tcp_server.h"
#include "tcp_conn.h"
#include "config_file.h"

// 所有在线的客户端
std::map<int, ylars::tcp_conn*> ylars::tcp_server::conns;
// 带释放的通信句柄
std::queue<ylars::tcp_conn*> ylars::tcp_server::_rel_conns;
// 释放通信句柄所需的锁
pthread_mutex_t ylars::tcp_server::_conn_mutex = PTHREAD_MUTEX_INITIALIZER;
// 激活释放操作
int ylars::tcp_server::_activatefd = eventfd(0, EFD_NONBLOCK);
// 是否需要激活
bool ylars::tcp_server::_isactivate = true;
// 消息路由
ylars::msg_router ylars::tcp_server::_router{};

// HOOK
ylars::conn_callback ylars::tcp_server::start_cb = nullptr;
void* ylars::tcp_server::start_cb_args = nullptr;
ylars::conn_callback ylars::tcp_server::end_cb = nullptr;
void* ylars::tcp_server::end_cb_args = nullptr;

void accept_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto ser = static_cast<ylars::tcp_server*>(args);
	ser->do_accept();
}

void release_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto ser = static_cast<ylars::tcp_server*>(args);
	ser->do_release();
}

ylars::tcp_server::tcp_server(event_loop* loop, const char* ip, const uint16_t port) :
	_loop{ loop },
	_max_conn_fd{ (int)config_file::instance()->GetNumber("reactor", "maxConn", 3) },
	_thread_pool{ nullptr }
{
	if (nullptr == loop)
	{
		std::cerr << "event_loop is nullptr" << std::endl;
		exit(1);
	}
	// 初始化释放激活句柄
	if (_activatefd < 0)
	{
		std::cerr << "activated create error" << std::endl;
		exit(1);
	}
	// 1. 排除SIGHUP和SIGPIPE两个信号
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
	{
		// 屏蔽信号失败
		std::cerr << "signal ignore error, signal is SIGHUP!" << std::endl;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		// 信号屏蔽失败
		std::cerr << "signal ignore error, signal is SIGPIPE!" << std::endl;
	}

	// 2. 创建套接字
	_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
	if (_sockfd < 0)
	{
		// 套接字创建失败
		std::cerr << "socket create failed !" << std::endl;
		exit(1);
	}

	// 3. 初始化服务器
	sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(ip, &addr.sin_addr);
	if (addr.sin_addr.s_addr < 0)
	{
		std::cerr << "ip addr error !" << std::endl;
		exit(-1);
	}

	// 4. 设置端口复用
	int opt = 1;
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::cerr << "set reuse addr error  ! error is " << strerror(errno) << std::endl;
	}

	// 5. 绑定端口
	if (bind(_sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		std::cerr << "bind port error ! error is " << strerror(errno) << std::endl;
		exit(1);
	}

	// 6. 设置监听
	if (listen(_sockfd, 500) < 0)
	{
		std::cerr << "listen error ! error is " << strerror(errno) << std::endl;
		exit(1);
	}

	// TODO 读配置文件，获取线程个数
	// 创建线程池
	int threadNum = (int)config_file::instance()->GetNumber("reactor", "threadNum", 2);
	if (threadNum > 0)
	{
		_thread_pool = new thread_pool{ threadNum };
		if (nullptr == _thread_pool)
		{
			// 线程池生成失败
			std::cerr << "new thread pool fail !" << std::endl;
		}
	}
	else
	{
		std::cout << "Single threaded mode !" << std::endl;
	}

	// 7. 加入事件循环
	loop->add_event(_sockfd, EPOLLIN, accept_cb, this);
	// 将释放激活事件加入事件循环
	loop->add_event(_activatefd, EPOLLIN, release_cb, this);
#if 0
	// 设置非阻塞
	int flag = fcntl(_activatefd, F_GETFL);
	fcntl(_activatefd, F_SETFL, flag | O_NONBLOCK);
#endif
}

ylars::tcp_server::~tcp_server()
{
	close(_sockfd);
}

void ylars::tcp_server::do_accept()
{
	sockaddr_in cli_addr;
	socklen_t addrlen = sizeof(cli_addr);
	bzero(&cli_addr, addrlen);
	int fd = accept(_sockfd, (sockaddr*)&cli_addr, &addrlen);
	if (fd < 0)
	{
		if (EINTR == errno)
		{
			// 中断错误
			std::cerr << "EINTR !" << std::endl;
			return;
		}
		else if (EAGAIN == errno)
		{
			// 非阻塞错误
			std::cerr << "EAGAIN !" << std::endl;
			return;
		}
		else if (EMFILE == errno)
		{
			// 当前连接过多，资源占用不够
			std::cerr << "Too much connection !" << std::endl;
			return;
		}
		else
		{
			std::cerr << "accept error ! errno is " << strerror(errno) << std::endl;
			exit(1);
		}
	}
	// 判断是否超过最大连接个数
	if (static_cast<int>(conns.size()) + 1 > _max_conn_fd)
	{
		std::cout << "Too many current connections !" << std::endl;
		close(fd);
		return;
	}
	if (nullptr != _thread_pool)
	{
		// 交由线程处理
		_thread_pool->add_conn_task(fd);
	}
	else {
		// 连接成功，将新连接加入到epoll事件中
		auto conn = new tcp_conn{ _loop, fd };
		if (nullptr != conn)
		{
			std::cout << "new client [" << fd << "]" << std::endl;
			increase_conn(fd, conn);
		}
		else
		{
			// 暂时无法连接
			std::cerr << "new tcp_conn error" << std::endl;
			close(fd);
		}
	}
}

void ylars::tcp_server::increase_conn(int connfd, tcp_conn* conn)
{
	pthread_mutex_lock(&_conn_mutex);
	conns[connfd] = conn;
	pthread_mutex_unlock(&_conn_mutex);
}

void ylars::tcp_server::decrease_conn(int connfd)
{
	char msg[16] = "need close~";
	pthread_mutex_lock(&_conn_mutex);
	auto conn = conns[connfd];
	conns.erase(connfd);
	_rel_conns.push(conn);
	if (_isactivate)
	{
		_isactivate = false;
		write(_activatefd, &msg, sizeof(msg));
	}
	pthread_mutex_unlock(&_conn_mutex);
}

int ylars::tcp_server::get_conn_nums()
{
	return conns.size();
}

void ylars::tcp_server::do_release()
{
	tcp_conn* conn = nullptr;
	while (true)
	{
		pthread_mutex_lock(&_conn_mutex);
		if (_rel_conns.empty())
		{
			pthread_mutex_unlock(&_conn_mutex);
			break;
		}
		conn = _rel_conns.front();
		_rel_conns.pop();
		pthread_mutex_unlock(&_conn_mutex);
		delete conn;
		conn = nullptr;
	}
	char msg[16] = { 0 };
	pthread_mutex_lock(&_conn_mutex);
	read(_activatefd, msg, sizeof(msg));
	_isactivate = true;
	pthread_mutex_unlock(&_conn_mutex);
}

ylars::thread_pool* ylars::tcp_server::get_thread_pool()
{
	return _thread_pool;
}

void ylars::tcp_server::register_msgid(int msgid, router_callback cb, void* args)
{
	pthread_mutex_lock(&_conn_mutex);
	_router.register_router(msgid, cb, args);
	pthread_mutex_unlock(&_conn_mutex);
}

ylars::msg_router& ylars::tcp_server::get_router()
{
	return _router;
}

void ylars::tcp_server::register_conn_start(conn_callback cb, void* args)
{
	pthread_mutex_lock(&_conn_mutex);
	start_cb = cb;
	start_cb_args = args;
	pthread_mutex_unlock(&_conn_mutex);
}

void ylars::tcp_server::register_conn_end(conn_callback cb, void* args)
{
	pthread_mutex_lock(&_conn_mutex);
	end_cb = cb;
	end_cb_args = args;
	pthread_mutex_unlock(&_conn_mutex);
}

void ylars::tcp_server::start_hook_call(conn_base* conn)
{
	if (nullptr != start_cb)
	{
		start_cb(conn, start_cb_args);
	}
}

void ylars::tcp_server::end_hook_call(conn_base* conn)
{
	if (nullptr != end_cb)
	{
		end_cb(conn, end_cb_args);
	}
}
