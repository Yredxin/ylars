#ifndef __LARS_REACTOR_BUF__
#define __LARS_REACTOR_BUF__

#include "io_buf.h"

/**
* 用于外界路由的消息对象
*/
namespace ylars
{
	class reactor_buf
	{
	public:
		reactor_buf();
		~reactor_buf();
		// 获取数据长度
		int length();
		// 弹出数据
		void pop(int len);
		// 清除数据，释放回收资源
		void clear();
	protected:
		io_buf* _buf;
	};

	// 读内存块操作
	class in_buf :public reactor_buf
	{
	public:
		// 从fd中读取数据到内存块
		int readfd(int fd);
		// 获取当前数据
		const char* get_data();
		// 重置缓冲区
		void adjust();
	};

	// 写内存块操作
	class out_buf :public reactor_buf
	{
	public:
		// 将输入放入到内存块
		int send_data(const void* data, int datalen);
		// 发送输入
		int write2fd(int fd);
	};
}



#endif // !__LARS_REACTOR_BUF__

