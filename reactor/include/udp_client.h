#ifndef __LARS_UDP_CLIENT__
#define __LARS_UDP_CLIENT__

#include <stdint.h>
#include "conn_base.h"
#include "event_loop.h"
#include "message.h"

namespace ylars
{
	class udp_client :public conn_base
	{
	public:
		udp_client(event_loop* loop, const char* ip, uint16_t port);
		~udp_client();
		// 通过 conn_base 继承
		virtual int send_msg(int msgid, const void* data, int datalen) override;
		virtual int get_fd() override;
		virtual void disconnect() override;
		void do_read();
		void register_msgid(int msgid, router_callback cb, void* args = nullptr);

	private:
		int _sockfd;
		event_loop* _loop;
		msg_router _router;
		char _read_buf[MSG_HEAD_LEN + MSG_MAX_LIMIT];
		char _write_buf[MSG_HEAD_LEN + MSG_MAX_LIMIT];
	};
}

#endif // !__LARS_UDP_CLIENT__
