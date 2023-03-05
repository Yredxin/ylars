#ifndef __LARS_CONN_BASE__
#define __LARS_CONN_BASE__

namespace ylars
{
	class conn_base
	{
	public:
		virtual int send_msg(int msgid, const void* data, int datalen) = 0;
		virtual int get_fd() = 0;
		virtual void disconnect() = 0;
		virtual ~conn_base() = default;

		// 用于和连接绑定参数
		void* param;
	};
	typedef void (*conn_callback)(ylars::conn_base* conn, void* args);
}

#endif // !__LARS_CONN_BASE__
