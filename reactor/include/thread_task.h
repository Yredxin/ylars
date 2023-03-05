#ifndef __LARS_THREAD_TASK__
#define __LARS_THREAD_TASK__

namespace ylars
{
	class event_loop;
	typedef void (*asyn_task_callback)(ylars::event_loop* loop, void* args);
	struct thread_task
	{
		enum task_type
		{
			NEW_CONN,	// 新连接
			NEW_TASK	// 新异步任务
		} type;

		union
		{
			int sockfd;
			struct
			{
				asyn_task_callback asyn_cb;
				void* args;
			};
		};
	};
}

#endif // !__LARS_THREAD_TASK__
