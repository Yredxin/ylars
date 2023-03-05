#ifndef __LARS_MESSAGE__
#define __LARS_MESSAGE__
#include <map>
#include "conn_base.h"

#define MSG_HEAD_LEN 8
#define MSG_MAX_LIMIT (65535 - MSG_HEAD_LEN)
namespace ylars
{
	struct msg_head
	{
		int msgid;
		int msglen;
	};

	typedef void (*router_callback)(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args);

	// 消息路由
	class msg_router
	{
	public:
		msg_router() = default;
		~msg_router() = default;

		void register_router(int msgid, router_callback cb, void* args);

		void call(conn_base* conn, int msgid, const void* msg, int msglen);

	private:
		std::map<int, router_callback> _router;
		std::map<int, void*> _args;
	};
}

#endif // !__LARS_MESSAGE__
