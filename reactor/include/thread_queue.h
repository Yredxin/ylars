#ifndef __LARS_THREAD_QUEUE__
#define __LARS_THREAD_QUEUE__
#include <unistd.h>
#include <iostream>
#include <sys/eventfd.h>
#include <queue>
#include "event_loop.h"

/**
* 任务队列
*/
namespace ylars
{
	template <typename T>
	class thread_queue
	{
	public:
		thread_queue() :
			_noticefd{ -1 },
			_loop{ nullptr }
		{
			// 创建一个监听描述符，不属于任何磁盘，或任何设备，仅用于通信
			_noticefd = eventfd(0, EFD_NONBLOCK);
			if (_noticefd < 0)
			{
				// 创建失败
				std::cerr << "create event fd error !" << std::endl;
				exit(1);
			}
			// 初始化线程锁
			pthread_mutex_init(&_task_mutex, nullptr);
		}

		~thread_queue()
		{
			pthread_mutex_destroy(&_task_mutex);
			close(_noticefd);
		}

		// 添加一个新任务
		void send_task(const T task)
		{
			pthread_mutex_lock(&_task_mutex);
			_tasks.push(task);
			// 激活任务处理操作
			uint64_t activate = 1;
			if (write(_noticefd, &activate, sizeof(activate)) < 0)
			{
				std::cerr << "activate task fail !" << std::endl;
			}
			pthread_mutex_unlock(&_task_mutex);
		}

		// 处理任务，tasks必须是一个空队列
		void recv_tasks(std::queue<T>& tasks)
		{
			uint64_t deal = 0;
			pthread_mutex_lock(&_task_mutex);
			_tasks.swap(tasks);
			if (read(_noticefd, &deal, sizeof(deal)) < 0)
			{
				std::cerr << "reset task fail !" << std::endl;
			}
			pthread_mutex_unlock(&_task_mutex);
		}

		// 设置事件监听器
		void set_event_loop(event_loop* loop)
		{
			_loop = loop;
		}

		// 设置任务处理回调
		void set_task_cb(event_callback cb, void* args = nullptr)
		{
			_loop->add_event(_noticefd, EPOLLIN, cb, args);
		}

	private:
		// 用于任务发布的文件描述符
		int _noticefd;
		// 任务队列
		std::queue<T> _tasks;
		// 线程锁
		pthread_mutex_t _task_mutex;
		// 事件监听器
		event_loop* _loop;
	};
}

#endif // !__LARS_THREAD_QUEUE__
