#ifndef __LARS_BUF_POOL__
#define __LARS_BUF_POOL__

#include <map>
#include <vector>
#include "io_buf.h"

// 单位为KB
#define MAX_BUF_POOL_CAP ( 4 * 1024 * 1024 )

namespace ylars
{
	// 全局单例
	class buf_pool
	{
	public:
		static buf_pool* instance();

		// 申请一块内存
		io_buf* alloc_buf(int len = m4K);

		// 释放一块内存
		void release(io_buf* buf);

	private:
		buf_pool();
		~buf_pool();

		enum _mem_type :int
		{
			m4K = 4096,
			m16K = 16384,
			m64K = 65536,
			m256K = 262144,
			m1M = 1048576,
			m4M = 4194304,
			m16M = 16777216
		};

		// 用于操作的内存池，对开分配和回收
		std::map<_mem_type, io_buf*> _pool;
		// 记录所有内存指针，用于释放
		// 单位为KB
		uint64_t _curr_cap;

		// 线程安全，互斥量
		static pthread_mutex_t _mem_mutex;

		// ==============构建内存==============
		void make_io_buf_pool(_mem_type type, int num);

		// ==============申请具体大小 ==============
		_mem_type get_mem_len(int len);

		// ==============单例对象生成==============
		// 单例对象
		static buf_pool* _ins_buf_pool;

		// 懒汉模式，生成全局唯一单例对象
		static void init();

		// 信号量，保证全局唯一
		static pthread_once_t _once;

	};

}

#endif // !__LARS_BUF_POOL__

