#ifndef __LARS_TCP_CONN__
#define __LARS_TCP_CONN__

#include "conn_base.h"
#include "reactor_buf.h"
#include "event_loop.h"
#include "tcp_server.h"

namespace ylars
{
	class tcp_conn :public conn_base
	{
	public:
		tcp_conn(event_loop* loop, int fd);
		~tcp_conn();

		// 主动发送数据
		virtual int send_msg(int msgid, const void* data, int datalen) override;

		void do_read();
		void do_write();

		// 通过 conn_base 继承
		virtual int get_fd() override;
		virtual void disconnect() override;
	private:
		event_loop* _loop;
		int _sockfd;

		in_buf _ibuf;
		out_buf _obuf;

		void clean();


	};
}

#endif // !__LARS_TCP_CONN__

