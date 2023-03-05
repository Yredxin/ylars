#include <sys/epoll.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "event_loop.h"

ylars::event_loop::event_loop()
{
	_epollfd = epoll_create1(0);
	if (_epollfd < 0)
	{
		std::cerr << "epoll_create1 error ! error is " << strerror(errno) << std::endl;
		exit(1);
	}
}

ylars::event_loop::~event_loop()
{
	close(_epollfd);
}

void ylars::event_loop::start_process()
{
	while (!_lfds.empty())
	{
		int num = epoll_wait(_epollfd, _fired_evs, MAXEVENTS, -1);
		if (num < 0)
		{
			// 出现错误
			std::cerr << "epoll_wait error ! error is " << strerror(errno) << std::endl;
			continue;
		}
		// 遍历事件集合
		for (int i = 0; i < num; i++)
		{
			auto fd_it = _lfds.find(_fired_evs[i].data.fd);
			auto evn_it = _io_evns.find(_fired_evs[i].data.fd);
			if (_lfds.end() == fd_it || _io_evns.end() == evn_it)
			{
				// 异常事件，从epoll中移除
				del_event(_fired_evs[i].data.fd);
				continue;
			}
			if (EPOLLIN & _fired_evs[i].events)
			{
				// 如果没有读回调，表示为异常事件，将其剔除
				if (nullptr == evn_it->second.read_cb)
				{
					del_event(_fired_evs[i].events, EPOLLIN);
					continue;
				}
				evn_it->second.read_cb(this, _fired_evs->data.fd, evn_it->second.read_args);
			}
			else if (EPOLLOUT & _fired_evs[i].events)
			{
				// 如果没有读回调，表示为异常事件，将其剔除
				if (nullptr == evn_it->second.write_cb)
				{
					del_event(_fired_evs[i].events, EPOLLOUT);
					continue;
				}
				evn_it->second.write_cb(this, _fired_evs->data.fd, evn_it->second.write_args);
			}
			else if ((EPOLLHUP | EPOLLERR) & _fired_evs[i].events)
			{
				// 水平触发未处理可能出现HUP，不是错误,需要正常处理
				if (nullptr != evn_it->second.read_cb)
				{
					evn_it->second.read_cb(this, _fired_evs->data.fd, evn_it->second.read_args);
				}
				else if (nullptr != evn_it->second.write_cb)
				{
					evn_it->second.read_cb(this, _fired_evs->data.fd, evn_it->second.read_args);
				}
				else
				{
					std::cerr << _fired_evs[i].data.fd << "get error !" << std::endl;
					del_event(_fired_evs[i].data.fd);
				}
			}
			else
			{
				std::cerr << _fired_evs[i].data.fd << "get error !" << std::endl;
				del_event(_fired_evs[i].data.fd);
			}
		}

		// 异步任务处理
		execute_async_tasks();
	}
}

void ylars::event_loop::del_event(int evnfd)
{
	_lfds.erase(evnfd);
	_io_evns.erase(evnfd);
	epoll_ctl(_epollfd, EPOLL_CTL_DEL, evnfd, nullptr);
}

void ylars::event_loop::del_event(int evnfd, int mask)
{
	auto fd_it = _lfds.find(evnfd);
	auto evn_it = _io_evns.find(evnfd);
	int new_mask = evn_it->second.mask & (~mask);
	if (new_mask <= 0 || _lfds.end() == fd_it || _io_evns.end() == evn_it)
	{
		// 不在集合中尝试将其删除或类型为0
		del_event(evnfd);
		return;
	}
	_io_evns[evnfd].mask = new_mask;
	epoll_event evn;
	evn.events = new_mask;
	evn.data.fd = evnfd;
	epoll_ctl(_epollfd, EPOLL_CTL_MOD, evnfd, &evn);
}

void ylars::event_loop::add_event(int evnfd, int mask, event_callback cb, void* args)
{
	int opt = EPOLL_CTL_ADD;
	int setmask = mask;
	auto evn_it = _io_evns.find(evnfd);
	if (evn_it != _io_evns.end())
	{
		// 已经存在，表示为修改
		opt = EPOLL_CTL_MOD;
		if (evn_it->second.mask & mask)
		{
			// 已经注册
			return;
		}
		setmask = evn_it->second.mask | mask;
	}
	// 添加新事件
	if (EPOLLIN & mask)
	{
		_io_evns[evnfd].mask = setmask;
		_io_evns[evnfd].read_cb = cb;
		_io_evns[evnfd].read_args = args;
	}
	else if (EPOLLOUT & mask)
	{
		_io_evns[evnfd].mask = setmask;
		_io_evns[evnfd].write_cb = cb;
		_io_evns[evnfd].write_args = args;
	}
	else
	{
		return;
	}
	_lfds.insert(evnfd);
	epoll_event evn;
	evn.events = setmask;
	evn.data.fd = evnfd;
	epoll_ctl(_epollfd, opt, evnfd, &evn);
}

void ylars::event_loop::add_event(int evnfd, int mask, event_callback rcb, void* rargs, event_callback wcb, void* wargs)
{
	add_event(evnfd, mask, rcb, rargs);
	add_event(evnfd, mask, wcb, wargs);
}

void ylars::event_loop::send_async_task(asyn_task_callback cb, void* args)
{
	// 新增任务
	_async_tasks.push_back({ cb, args });
}

void ylars::event_loop::execute_async_tasks()
{
	for (asyn_task_t& task : _async_tasks)
	{
		task.first(this, task.second);
	}
	_async_tasks.clear();
}

void ylars::event_loop::get_listen_fds(evn_set_t& fds)
{
	fds = _lfds;
}
