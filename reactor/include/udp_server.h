#ifndef __LARS_UDP_SERVER__
#define __LARS_UDP_SERVER__

#include <netinet/in.h>
#include "event_loop.h"
#include "conn_base.h"
#include "message.h"

namespace ylars
{
	class udp_server :public conn_base
	{
	public:
		udp_server(event_loop* loop, const char* ip, uint16_t port);
		~udp_server();

		// 通过 conn_base 继承
		virtual int send_msg(int msgid, const void* data, int datalen) override;

		// 注册函数
		void register_msgid(int msgid, router_callback cb, void* args = nullptr);

		virtual int get_fd() override;

		virtual void disconnect() override;
		void do_read();
	private:
		// 监听描述符
		int _sockfd;
		// 事件循环器
		event_loop* _loop;
		// 客户端地址
		sockaddr_in _cli_addr;
		// 客户端地址长度
		socklen_t _cli_addr_len;
		char _read_buf[MSG_HEAD_LEN + MSG_MAX_LIMIT];
		char _write_buf[MSG_HEAD_LEN + MSG_MAX_LIMIT];
		// 路由机制
		msg_router _router;
	};
}

#endif // !__LARS_UDP_SERVER__
