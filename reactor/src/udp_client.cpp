#include <iostream>
#include <cstring>
#include <strings.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <unistd.h>
#include "udp_client.h"

void udp_cli_read_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto cli = static_cast<ylars::udp_client*>(args);
	cli->do_read();
}

ylars::udp_client::udp_client(event_loop* loop, const char* ip, uint16_t port) :
	_sockfd{ -1 },
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
	_sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
	if (_sockfd < 0)
	{
		std::cerr << "create socket error !" << std::endl;
		exit(1);
	}
	// 初始化服务器地址
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(ip, &addr.sin_addr);

	// 连接服务器
	if (connect(_sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		std::cerr << "connection to " << ip << ":" << port << " error !" << std::endl;
		exit(1);
	}
	// 添加到事件监听
	_loop->add_event(_sockfd, EPOLLIN, udp_cli_read_cb, this);
}

ylars::udp_client::~udp_client()
{
	_loop->del_event(_sockfd);
	close(_sockfd);
}

int ylars::udp_client::send_msg(int msgid, const void* data, int datalen)
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
	int sendlen = sendto(_sockfd, _write_buf, datalen + MSG_HEAD_LEN, 0, nullptr, 0);
	if (sendlen < 0)
	{
		perror("sendto() ...");
		return -1;
	}
	return sendlen;
}

void ylars::udp_client::disconnect()
{
	_loop->del_event(_sockfd);
	close(_sockfd);
	exit(0);
}

void ylars::udp_client::do_read()
{
	while (true)
	{
		bzero(_read_buf, sizeof(_read_buf));
		int datalen = recvfrom(_sockfd, _read_buf, sizeof(_read_buf), 0, nullptr, nullptr);
		if (datalen < 0)
		{
			if (EINTR == errno)
			{
				// 中断错误
				continue;
			}
			else if (EAGAIN == errno)
			{
				// 非阻塞
				break;
			}
			else
			{
				perror("udp server recv from error !");
				break;
			}
		}
		msg_head head;
		memcpy(&head, _read_buf, MSG_HEAD_LEN);
		if (head.msglen > MSG_MAX_LIMIT || head.msglen < 0 || head.msglen + MSG_HEAD_LEN != datalen)
		{
			// 异常数据
			fprintf(stderr, "error message head format error\n");
			continue;
		}
		_router.call(this, head.msgid, _read_buf + MSG_HEAD_LEN, head.msglen);
	}
}

void ylars::udp_client::register_msgid(int msgid, router_callback cb, void* args)
{
	_router.register_router(msgid, cb, args);
}

int ylars::udp_client::get_fd()
{
	return _sockfd;
}
