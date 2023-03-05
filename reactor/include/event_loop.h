#ifndef __LARS_EVENT_LOOP__
#define __LARS_EVENT_LOOP__

#include <sys/epoll.h>
#include <map>
#include <set>
#include <vector>
#include "thread_task.h"
#include "epoll_base.h"

/* 每次最多获取文件描述符个数 */
#define MAXEVENTS 10

/**
* 事件处理
*/
namespace ylars
{
	typedef std::map<int, io_event> evn_map_t;
	typedef std::set<int> evn_set_t;
	class event_loop
	{
	public:
		event_loop();
		~event_loop();

		/* 启动事件循环 */
		void start_process();

		/* 移除一个事件 */
		void del_event(int evnfd);
		/* 移除一个事件的一个类型 */
		void del_event(int evnfd, int mask);

		/* 新增一个事件 */
		void add_event(int evnfd, int mask, event_callback cb, void* args = nullptr);
		void add_event(int evnfd, int mask, event_callback rcb, void* rargs, event_callback wcb, void* wargs);

		/* 添加一个异步任务 */
		void send_async_task(asyn_task_callback cb, void* args = nullptr);

		/* 执行所有异步任务 */
		void execute_async_tasks();

		// 获取当前监听的文件描述符
		void get_listen_fds(evn_set_t& fds);
	private:
		int _epollfd;						// epoll 句柄
		epoll_event _fired_evs[MAXEVENTS];	// 每次触发的事件集合
		evn_map_t _io_evns;					// 事件集合
		evn_set_t _lfds;					// 监听套接字集合

		// 异步任务处理集合
		typedef	std::pair<asyn_task_callback, void*> asyn_task_t;
		std::vector<asyn_task_t> _async_tasks; // 异步任务集合
	};
}

#endif // !__LARS_EVENT_LOOP__
