#ifndef __LARS_IO_BUF__
#define __LARS_IO_BUF__

#include <stdint.h>
#include <pthread.h>

/**
* 内存块结构体
*/

namespace ylars
{
	struct io_buf
	{
		explicit io_buf(int capacity);
		io_buf(const io_buf& buf);
		io_buf& operator=(const io_buf& buf);
		~io_buf();

		// 弹出N字节数据，N <= _datalen
		void pop(int n);
		// 重置内存，每次pop完之后建议重置，一遍减少内存获取
		void adjust();
		// 拷贝操作
		void copy(const io_buf* buf);
		// 容量
		int capacity;
		// 用户数据长度
		int datalen;
		// 数据内存块首地址
		char* data;
		// 当前数据开始地址位置，如果datalen == 65536，0 <= head <= 65535
		int head;
		// 下一个节点位置
		io_buf* next;

#if 0	// 不需要锁
	private:
		// 内存锁
		pthread_mutex_t _mem_mutex;
#endif
	};
}

#endif // !__LARS_IO_BUF__
