#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <cstring>
#include "message.h"
#include "event_loop.h"
#include "tcp_client.h"

void write_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto cli = static_cast<ylars::tcp_client*>(args);
	cli->do_write();
}

void tcp_cli_read_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto cli = static_cast<ylars::tcp_client*>(args);
	cli->do_read();
}

void connect_ser(ylars::event_loop* loop, int fd, void* args)
{
	loop->del_event(fd, EPOLLOUT);
	auto cli = static_cast<ylars::tcp_client*>(args);
	if (nullptr == cli)
	{
		std::cerr << "Exception error !" << std::endl;
		return;
	}
	int res = 0;
	socklen_t reslen = sizeof(res);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &reslen);
	if (0 == res)
	{
		// 连接成功
		std::cout << "connection success" << std::endl;
		loop->add_event(fd, EPOLLIN, tcp_cli_read_cb, cli);
		if (cli->obuf.length() > 0)
		{
			loop->add_event(fd, EPOLLOUT, write_cb, cli);
		}
		// 调用HOOK
		cli->start_hook_call();
	}
	else
	{
		// 连接失败
		std::cerr << "connection fail !" << std::endl;
		cli->disconnect();
		return;
	}
}

ylars::tcp_client::tcp_client(event_loop* loop, const char* ip, uint16_t port) :
	_sockfd{ -1 },
	_loop{ loop },
	start_cb{ nullptr },
	start_cb_args{ nullptr },
	end_cb{ nullptr },
	end_cb_args{ nullptr },
	ibuf{},
	obuf{}
{
	// 屏蔽信号
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
	{
		std::cerr << "signal ignore error, signal is SIGHUP!" << std::endl;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		std::cerr << "signal ignore error, signal is SIGPIPE!" << std::endl;
	}

	// 创建套接字
	_sockfd = socket(AF_INET, SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd < 0)
	{
		std::cerr << "create socket error !" << std::endl;
		exit(1);
	}

	// 初始化连接服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(ip, &addr.sin_addr);

	std::cout << "Connecting to the [" << ip << ":" << port << "] ..." << std::endl;
	// 尝试连接
	int ret = connect(_sockfd, (sockaddr*)&addr, sizeof(addr));
	if (ret == 0)
	{
		// 连接成功
		connect_ser(_loop, _sockfd, this);
	}
	else if (ret < 0)
	{
		if (EINPROGRESS == errno)
		{
			// 非阻塞连接，判断是否可写，可写表示连接成功
			loop->add_event(_sockfd, EPOLLOUT, connect_ser, this);
		}
		else
		{
			// 连接失败
			std::cerr << "connect fail !" << std::endl;
			disconnect();
			// TODO 重连
			return;
		}
	}
}

ylars::tcp_client::~tcp_client()
{
	clean();
}

int ylars::tcp_client::send_msg(int msgid, const void* data, int datalen)
{
	msg_head head;
	head.msgid = msgid;
	head.msglen = datalen;
	int ret = obuf.send_data(&head, MSG_HEAD_LEN);
	if (ret < 0)
	{
		// 写入数据失败
		std::cerr << "send msg error !" << std::endl;
		return ret;
	}
	ret = obuf.send_data(data, datalen);
	if (ret < 0)
	{
		// 写入数据失败
		std::cerr << "send msg error !" << std::endl;
		return ret;
	}
	_loop->add_event(_sockfd, EPOLLOUT, write_cb, this);
	return ret;
}

void ylars::tcp_client::do_read()
{
	// 读数据
	int readlen = ibuf.readfd(_sockfd);
	if (0 == readlen)
	{
		// 服务器断开连接
		std::cout << "Stopping the server service ~" << std::endl;
		disconnect();
		// TODO 重连
		return;
	}
	else if (readlen < 0)
	{
		// 写数据失败
		std::cerr << "send msg error !" << std::endl;
		clean();
		return;
	}

	// 解析数据
	msg_head head;
	while (ibuf.length() >= MSG_HEAD_LEN)
	{
		memcpy(&head, ibuf.get_data(), MSG_HEAD_LEN);
		if (head.msglen > _SC_NL_MSGMAX || head.msglen < 0)
		{
			// 异常数据
			std::cerr << "Abnormal data !" << std::endl;
			clean();
			return;
		}
		if (ibuf.length() - MSG_HEAD_LEN < head.msglen)
		{
			// 数据未读取完毕
			break;
		}
		ibuf.pop(MSG_HEAD_LEN);

#if 0
		std::cout << "server send data: ";
		write(STDOUT_FILENO, ibuf.get_data(), head.msglen);
		fflush(stdout);
		std::cout << std::endl;
		send_msg(head.msgid, ibuf.get_data(), ibuf.length());
#endif
		// 消息路由
		_router.call(this, head.msgid, ibuf.get_data(), head.msglen);
		// 弹出数据
		ibuf.pop(head.msglen);
	}
	ibuf.adjust();
}

void ylars::tcp_client::do_write()
{
	while (obuf.length() > 0)
	{
		int ret = obuf.write2fd(_sockfd);
		if (ret < 0)
		{
			std::cerr << "send data error !" << std::endl;
			clean();
			return;
		}
		else if (ret == 0)
		{
			// 暂时不可写
			return;
		}
	}
	_loop->del_event(_sockfd, EPOLLOUT);
	return;
}

void ylars::tcp_client::register_msgid(int msgid, router_callback cb, void* args)
{
	_router.register_router(msgid, cb, args);
}

void ylars::tcp_client::clean()
{
	if (_sockfd >= 0)
	{
		ibuf.clear();
		obuf.clear();
		_loop->del_event(_sockfd);

		// HOOK结束调用
		end_hook_call();

		close(_sockfd);
		_sockfd = -1;
	}
}

void ylars::tcp_client::disconnect()
{
	clean();
}

void ylars::tcp_client::register_conn_start(conn_callback cb, void* args)
{
	start_cb = cb;
	start_cb_args = args;
}

void ylars::tcp_client::register_conn_end(conn_callback cb, void* args)
{
	end_cb = cb;
	end_cb_args = args;
}

void ylars::tcp_client::start_hook_call()
{
	if (nullptr != start_cb)
	{
		start_cb(this, start_cb_args);
	}
}

void ylars::tcp_client::end_hook_call()
{
	if (nullptr != end_cb)
	{
		end_cb(this, end_cb_args);
	}
}

int ylars::tcp_client::get_fd()
{
	return _sockfd;
}
