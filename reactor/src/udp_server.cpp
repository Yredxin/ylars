#include <sys/socket.h>
#include <cstring>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include "udp_server.h"
#include "epoll_base.h"

void udp_ser_read_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto ser = static_cast<ylars::udp_server*>(args);
	ser->do_read();
}

ylars::udp_server::udp_server(event_loop* loop, const char* ip, uint16_t port) :
	_loop{ loop }
{
	if (nullptr == _loop)
	{
		std::cerr << "event_loop is nullptr" << std::endl;
		exit(1);
	}
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
	{
		// 屏蔽信号失败
		std::cerr << "signal ignore error, signal is sighup!" << std::endl;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		// 信号屏蔽失败
		std::cerr << "signal ignore error, signal is sigpipe!" << std::endl;
	}
	// 创建套接字
	_sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_UDP);
	if (_sockfd < 0)
	{
		std::cerr << "create socket error !" << std::endl;
		exit(1);
	}
	// 初始化服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(ip, &addr.sin_addr);
	// 设置端口复用
	int op = 1;
	setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));

	// 绑定端口
	if (bind(_sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		std::cerr << "bind port error" << std::endl;
	}
	// 清空地址
	bzero(&_cli_addr, sizeof(_cli_addr));
	_cli_addr_len = sizeof(_cli_addr);
	// 加入事件循环
	_loop->add_event(_sockfd, EPOLLIN, udp_ser_read_cb, this);
}

ylars::udp_server::~udp_server()
{
	close(_sockfd);
}

void ylars::udp_server::register_msgid(int msgid, router_callback cb, void* args)
{
	_router.register_router(msgid, cb, args);
}

int ylars::udp_server::send_msg(int msgid, const void* data, int datalen)
{

	if (datalen > MSG_MAX_LIMIT)
	{
		fprintf(stderr, "The message is too long\n");
		return -1;
	}
	bzero(_write_buf, sizeof(_write_buf));
	msg_head head;
	head.msgid = msgid;
	head.msglen = datalen;
	memcpy(_write_buf, &head, MSG_HEAD_LEN);

	memcpy(_write_buf + MSG_HEAD_LEN, data, datalen);
	int sendlen = sendto(_sockfd, _write_buf, static_cast<size_t>(datalen) + MSG_HEAD_LEN, 0, (struct sockaddr*)&_cli_addr, _cli_addr_len);
	if (sendlen < 0)
	{
		perror("sendto() ...");
		return -1;
	}
	return sendlen;
}

void ylars::udp_server::disconnect()
{
	close(_sockfd);
	exit(0);
}

void ylars::udp_server::do_read()
{
	while (true)
	{
		bzero(_read_buf, sizeof(_read_buf));
		int datalen = recvfrom(_sockfd, _read_buf, sizeof(_read_buf), 0, (struct sockaddr*)&_cli_addr, &_cli_addr_len);

		if (datalen < 0)
		{
			if (EINTR == errno)
			{
				// 中断错误
				continue;
			}
			else if (EAGAIN == errno)
			{
				// 非阻塞错误
				break;
			}
			else
			{
				perror("udp server recv from error !");
				break;
			}
		}
		msg_head head;
		memcpy(&head, _read_buf, sizeof(head));
		if (head.msglen > MSG_MAX_LIMIT || head.msglen < 0 || head.msglen + MSG_HEAD_LEN != datalen)
		{
			// 异常数据
			fprintf(stderr, "error message head format error\n");
			continue;
		}
		// 调用用户注册的回调
		_router.call(this, head.msgid, _read_buf + MSG_HEAD_LEN, head.msglen);
	}
}

int ylars::udp_server::get_fd()
{
	return _sockfd;
}
