#include <iostream>
#include <fstream>
#include "buf_pool.h"

ylars::buf_pool* ylars::buf_pool::_ins_buf_pool = nullptr;
pthread_once_t ylars::buf_pool::_once = PTHREAD_ONCE_INIT;
pthread_mutex_t ylars::buf_pool::_mem_mutex = PTHREAD_MUTEX_INITIALIZER;

ylars::buf_pool* ylars::buf_pool::instance()
{
	pthread_once(&_once, init);
	return _ins_buf_pool;
}

ylars::io_buf* ylars::buf_pool::alloc_buf(int len)
{
	pthread_mutex_lock(&_mem_mutex);
	_mem_type type = get_mem_len(len);
	io_buf* ret_buf = nullptr;
	// 有足够的内存申请
	if (nullptr != _pool[type])
	{
		ret_buf = _pool[type];
		_pool[type] = _pool[type]->next;
	}
	else
	{
		// 没有足够空间，重新申请或往上走一级
		if (_curr_cap + type / 1024 > MAX_BUF_POOL_CAP)
		{
			std::cerr << "The maximum memory capacity is exceeded !" << std::endl;
		}
		else
		{
			// 可以继续申请
			ret_buf = new io_buf{ type };
			if (nullptr == ret_buf)
			{
				std::cerr << "[" << __FILE__ << ":" << __LINE__ << "]" << "new io_buf error !" << std::endl;
			}
			_curr_cap += type / 1024;
		}
	}
	if (nullptr != ret_buf)
	{
		ret_buf->next = nullptr;
	}
	pthread_mutex_unlock(&_mem_mutex);
	return ret_buf;
}

void ylars::buf_pool::release(io_buf* buf)
{
	if (nullptr != buf)
	{
		// 重置数据
		buf->pop(buf->datalen);
		buf->adjust();
		buf->datalen = 0;

		// 放回
		pthread_mutex_lock(&_mem_mutex);
		_mem_type type = static_cast<_mem_type>(buf->capacity);
		buf->next = _pool[type];
		_pool[type] = buf;
		pthread_mutex_unlock(&_mem_mutex);
	}
}

ylars::buf_pool::buf_pool() :
	_curr_cap{ 0 }
{
	// 约576M
	make_io_buf_pool(m4K, 5000);  // 20000kb
	make_io_buf_pool(m16K, 1000); // 16000kb
	make_io_buf_pool(m64K, 500);	// 32000kb
	make_io_buf_pool(m256K, 200);	// 51200kb
	make_io_buf_pool(m1M, 100);	// 102400kb
	make_io_buf_pool(m4M, 50);	// 204800kb
	make_io_buf_pool(m16M, 10);	// 163840kb
	std::cout << "开辟内存 : " << _curr_cap / 1024 << "M" << std::endl;
}

ylars::buf_pool::~buf_pool()
{
}

void ylars::buf_pool::init()
{
	_ins_buf_pool = new buf_pool{};
}

void ylars::buf_pool::make_io_buf_pool(_mem_type type, int num)
{
	if ((type / 1024) * num + _curr_cap > MAX_BUF_POOL_CAP)
	{
		std::cerr << "The maximum memory capacity is exceeded !" << std::endl;
		return;
	}
	// 开辟头节点
	_pool[type] = new io_buf{ type };
	if (nullptr == _pool[type])
	{
		std::cerr << "[" << __FILE__ << ":" << __LINE__ << "]" << "new io_buf error !" << std::endl;
		exit(1);
	}
	auto curr = _pool[type];
	for (int i = 0; i < num - 1; i++)
	{
		curr->next = new io_buf{ type };
		if (nullptr == curr->next)
		{
			std::cerr << "[" << __FILE__ << ":" << __LINE__ << "]" << "new io_buf error !" << std::endl;
			exit(1);
		}
		curr = curr->next;
	}
	_curr_cap += (type / 1024) * num;
}

ylars::buf_pool::_mem_type ylars::buf_pool::get_mem_len(int len)
{
	_mem_type ret_type = m4K;
	if (len <= m4K)
	{
		ret_type = m4K;
	}
	else if (len <= m16K)
	{
		ret_type = m16K;
	}
	else if (len <= m64K)
	{
		ret_type = m64K;
	}
	else if (len <= m256K)
	{
		ret_type = m256K;
	}
	else if (len <= m1M)
	{
		ret_type = m1M;
	}
	else if (len <= m4M)
	{
		ret_type = m4M;
	}
	else if (len <= m16M)
	{
		ret_type = m16M;
	}
	return ret_type;
}

