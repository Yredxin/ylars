#ifndef __LARS_THREAD_POOL__
#define __LARS_THREAD_POOL__
#include <pthread.h>
#include <queue>
#include "thread_queue.h"
#include "thread_task.h"

namespace ylars
{
	class thread_pool
	{
	public:
		thread_pool(int thread_n);

		~thread_pool();

		// 添加一个新连接
		void add_conn_task(int sockfd);

		// 添加一个异步任务
		void add_async_task(asyn_task_callback cb, void* args = nullptr);
	private:
		// 线程个数
		int _thread_num;
		// 线程任务队列
		thread_queue<thread_task>** _task_queue;
		// 线程集合
		pthread_t* _threads;
		// 轮询任务索引
		int _polling;
		// 锁
		pthread_mutex_t _pool_mutex;
	};
}

#endif // !__LARS_THREAD_POOL__
