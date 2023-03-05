#ifndef __LARS_EVENT_BASE__
#define __LARS_EVENT_BASE__

/* epoll中存储的消息 */
namespace ylars
{
	class event_loop;
	typedef void (*event_callback)(ylars::event_loop* loop, int fd, void* args);
	struct io_event
	{
		io_event() :mask{ -1 }, read_cb{ nullptr }, read_args{ nullptr }, write_cb{ nullptr }, write_args{ nullptr } {};
		/* 消息类型，EPOLLIN EPOLLOUT EPOLLIN|EPOLLOUT */
		int mask;
		event_callback read_cb;
		void* read_args;
		event_callback write_cb;
		void* write_args;
	};
}

#endif // !__LARS_EVENT_BASE__
