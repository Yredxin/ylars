#include <unistd.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "tcp_conn.h"
#include "message.h"

void tcp_ser_read_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto ser = static_cast<ylars::tcp_conn*>(args);
	ser->do_read();
}

void write_cb(ylars::event_loop* loop, int fd, void* args)
{
	auto ser = static_cast<ylars::tcp_conn*>(args);
	ser->do_write();
}

ylars::tcp_conn::tcp_conn(event_loop* loop, int fd) :
	_loop{ loop },
	_sockfd{ fd }
{
	// 设置非阻塞
	int flag = fcntl(_sockfd, F_GETFL);
	fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
	// 禁止读写缓存，支持小消息触发
	int op = 1;
	setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));
	// 注册read事件
	_loop->add_event(_sockfd, EPOLLIN, tcp_ser_read_cb, this);
	// 调用HOOK
	tcp_server::start_hook_call(this);
}

ylars::tcp_conn::~tcp_conn()
{
	close(_sockfd);
	_sockfd = -1;
	clean();
}

void ylars::tcp_conn::clean()
{
	if (_sockfd > 0)
	{
		// 调用结束HOOK
		tcp_server::end_hook_call(this);
		_ibuf.clear();
		_obuf.clear();
		_loop->del_event(_sockfd);

		// 标注要释放,表示此链接对象将要被释放
		tcp_server::decrease_conn(_sockfd);
	}
}

void ylars::tcp_conn::disconnect()
{
	clean();
}

int ylars::tcp_conn::send_msg(int msgid, const void* data, int datalen)
{
	// 组包
	msg_head head;
	head.msgid = msgid;
	head.msglen = datalen;
	int ret = _obuf.send_data(&head, MSG_HEAD_LEN);
	if (ret < 0)
	{
		std::cerr << "data cache error !" << std::endl;
		return ret;
	}
	ret = _obuf.send_data(data, datalen);
	if (ret < 0)
	{
		std::cerr << "data cache error !" << std::endl;
		return ret;
	}
	_loop->add_event(_sockfd, EPOLLOUT, write_cb, this);
	return ret;
}

void ylars::tcp_conn::do_read()
{
	int readlen = _ibuf.readfd(_sockfd);
	if (0 == readlen)
	{
		// 对端关闭
		std::cout << "client [" << _sockfd << "] closed ~" << std::endl;
		clean();
		return;
	}
	else if (readlen < 0)
	{
		// 读取错误
		std::cerr << "read error !" << std::endl;
		clean();
		return;
	}
	msg_head head;
	while (_ibuf.length() >= MSG_HEAD_LEN)
	{
		// 取出消息头
		bzero(&head, MSG_HEAD_LEN);
		memcpy(&head, _ibuf.get_data(), MSG_HEAD_LEN);
		if (head.msglen > MSG_MAX_LIMIT || head.msglen < 0)
		{
			std::cerr << "Abnormal data !" << std::endl;
			clean();
			return;
		}
		if (_ibuf.length() - MSG_HEAD_LEN < head.msglen)
		{
			// 数据未读取完
			break;
		}
		_ibuf.pop(MSG_HEAD_LEN);

		// 一个包的数据读取完毕，用户操作
		// 消息路由
		tcp_server::get_router().call(this, head.msgid, _ibuf.get_data(), head.msglen);

		_ibuf.pop(head.msglen);
	}

	_ibuf.adjust();
}

void ylars::tcp_conn::do_write()
{
	while (_obuf.length() > 0)
	{
		int ret = _obuf.write2fd(_sockfd);
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

int ylars::tcp_conn::get_fd()
{
	return _sockfd;
}
