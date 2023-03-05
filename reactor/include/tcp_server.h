#ifndef __YLARS_TCP_SERVER__
#define __YLARS_TCP_SERVER__

#include <stdint.h>
#include <queue>
#include <list>
#include "event_loop.h"
#include "reactor_buf.h"
#include "message.h"
#include "thread_pool.h"

// #define MAX_CONNFD_NUM 100
/**
* tcp服务器
*/
namespace ylars
{
	class tcp_conn;
	class tcp_server
	{
	public:
		/* 初始化服务器，并设置监听 */
		tcp_server(event_loop* loop, const char* ip, const uint16_t port);
		~tcp_server();

		// 连接处理
		void do_accept();

		// 创建一个连接
		static void increase_conn(int connfd, tcp_conn* conn);

		// 销毁一个连接
		static void decrease_conn(int connfd);

		// 获取当前连接个数
		static int get_conn_nums();

		// 释放操作
		void do_release();

		// 获取当前线程池
		thread_pool* get_thread_pool();

		// 所有的连接
		static std::map<int, tcp_conn*> conns;

		// 注册消息
		static void register_msgid(int msgid, router_callback cb, void* args = nullptr);
		// 过去消息路由句柄
		static msg_router& get_router();

		// 注册启动HOOK
		static void register_conn_start(conn_callback cb, void* args = nullptr);
		static void register_conn_end(conn_callback cb, void* args = nullptr);
		static void start_hook_call(conn_base* conn);
		static void end_hook_call(conn_base* conn);
	private:
		int _sockfd;
		event_loop* _loop;
		int _max_conn_fd;
		// 线程池
		thread_pool* _thread_pool;

		// 存储每个客户端的连接
		static std::queue<tcp_conn*> _rel_conns;
		static pthread_mutex_t _conn_mutex;
		static int _activatefd;
		static bool _isactivate;
		static msg_router _router;

		// ===== HOOK
		static conn_callback start_cb;
		static void* start_cb_args;
		static conn_callback end_cb;
		static void* end_cb_args;
	};
}

#endif // !__YLARS_TCP_SERVER__
