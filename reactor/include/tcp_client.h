#ifndef __LARS_TCP_CLIENT__
#define __LARS_TCP_CLIENT__
#include <stdint.h>
#include "reactor_buf.h"
#include "conn_base.h"
#include "message.h"

namespace ylars
{
	class event_loop;
	class tcp_client :public conn_base
	{
	public:
		tcp_client(event_loop* loop, const char* ip, uint16_t port);
		~tcp_client();

		// 主动发送数据
		virtual int send_msg(int msgid, const void* data, int datalen) override;

		// 读取
		void do_read();

		// 写数据
		void do_write();

		// 注册消息路由
		void register_msgid(int msgid, router_callback cb, void* args = nullptr);

		// 通过 conn_base 继承
		virtual int get_fd() override;
		virtual void disconnect() override;


		// 注册HOOK
		void register_conn_start(conn_callback cb, void* args = nullptr);
		void register_conn_end(conn_callback cb, void* args = nullptr);
		void start_hook_call();
		void end_hook_call();
	private:
		int _sockfd;
		event_loop* _loop;
		msg_router _router;

		// 清除所有数据
		void clean();

		conn_callback start_cb;
		void* start_cb_args;
		conn_callback end_cb;
		void* end_cb_args;
	public:
		// 读缓冲区
		in_buf ibuf;

		// 写缓冲区
		out_buf obuf;


	};
}

#endif // !__LARS_TCP_CLIENT__
