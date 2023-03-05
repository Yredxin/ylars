#include <iostream>
#include "thread_pool.h"
#include "event_loop.h"
#include "tcp_server.h"
#include "tcp_conn.h"


void deal_task(ylars::event_loop* loop, int fd, void* args)
{
	auto task_queue = static_cast<ylars::thread_queue<ylars::thread_task>*>(args);
	if (nullptr == task_queue)
	{
		return;
	}
	// 获取任务
	std::queue<ylars::thread_task> tasks;
	task_queue->recv_tasks(tasks);
	// 遍历队列根据类型进行不同操作
	while (!tasks.empty())
	{
		auto task = tasks.front();
		tasks.pop();
		if (ylars::thread_task::NEW_CONN == task.type)
		{
			// 新连接
			auto conn = new ylars::tcp_conn{ loop, task.sockfd };
			if (nullptr == conn)
			{
				std::cerr << "create tcp conn error !" << std::endl;
				return;
			}
			ylars::tcp_server::increase_conn(task.sockfd, conn);

			std::cout << "Thread " << pthread_self() << " handles connection " << task.sockfd << std::endl;
		}
		else if (ylars::thread_task::NEW_TASK == task.type)
		{
			// 异步任务处理
			loop->send_async_task(task.asyn_cb, task.args);
		}
	}
}

void* thread_main(void* args)
{
	auto task_queue = static_cast<ylars::thread_queue<ylars::thread_task>*>(args);
	if (nullptr == task_queue)
	{
		return nullptr;
	}

	// 创建事件监听器
	ylars::event_loop loop{};
	// 设置任务事件监听器
	task_queue->set_event_loop(&loop);
	// 设置任务处理回调
	task_queue->set_task_cb(deal_task, task_queue);
	loop.start_process();
	return nullptr;
}

ylars::thread_pool::thread_pool(int thread_n) :
	_thread_num{ thread_n },
	_threads{ nullptr },
	_polling{ 0 }
{
	// 初始化锁
	pthread_mutex_init(&_pool_mutex, nullptr);
	if (thread_n < 0)
	{
		std::cerr << "Thread count error ! " << std::endl;
		exit(1);
	}
	// 创建线程集 
	_threads = new pthread_t[thread_n];
	if (nullptr == _threads)
	{
		std::cerr << "new threads error !" << std::endl;
		exit(1);
	}
	// 创建任务队列集
	_task_queue = new thread_queue<thread_task>*[thread_n];
	if (nullptr == _task_queue)
	{
		std::cerr << "new task queue error !" << std::endl;
		exit(1);
	}
	int ret = 0;
	for (int i = 0; i < thread_n; i++)
	{
		// 创建线程对应任务队列
		_task_queue[i] = new thread_queue<thread_task>{};
		ret = pthread_create(&_threads[i], nullptr, thread_main, _task_queue[i]);
		if (ret < 0)
		{
			// 线程创建失败
			std::cerr << "create thread error !" << std::endl;
			exit(1);
		}
		// 线程分离
		if (0 != pthread_detach(_threads[i]))
		{
			std::cerr << _threads[i] << " thread detach fail !" << std::endl;
		}
		std::cout << "Thread " << _threads[i] << " waits for work ..." << std::endl;
	}
}

ylars::thread_pool::~thread_pool()
{
	if (nullptr != _threads)
	{
		delete[] _threads;
	}
	pthread_mutex_destroy(&_pool_mutex);
}

// 添加一个新连接
void ylars::thread_pool::add_conn_task(int sockfd)
{
	thread_task task;
	task.type = thread_task::NEW_CONN;
	task.sockfd = sockfd;
	pthread_mutex_lock(&_pool_mutex);
	_task_queue[_polling++]->send_task(task);
	if (_polling >= _thread_num)
	{
		_polling = 0;
	}
	pthread_mutex_unlock(&_pool_mutex);
}

// 所有线程添加异步任务
void ylars::thread_pool::add_async_task(asyn_task_callback cb, void* args)
{
	thread_task task;
	task.type = thread_task::NEW_TASK;
	task.asyn_cb = cb;
	task.args = args;
	for (int i = 0; i < _thread_num; i++)
	{
		pthread_mutex_lock(&_pool_mutex);
		_task_queue[i]->send_task(task);
		pthread_mutex_unlock(&_pool_mutex);
	}
}

