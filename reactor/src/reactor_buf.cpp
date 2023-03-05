#include <cstring>
#include <sys/ioctl.h>
#include <iostream>
#include <unistd.h>
#include <assert.h>
#include "reactor_buf.h"
#include "buf_pool.h"

ylars::reactor_buf::reactor_buf() :
	_buf{ nullptr }
{
}

ylars::reactor_buf::~reactor_buf()
{
	clear();
}

int ylars::reactor_buf::length()
{
	if (nullptr != _buf)
	{
		return _buf->datalen;
	}
	return 0;
}

void ylars::reactor_buf::pop(int len)
{
	if (nullptr != _buf)
	{
		_buf->pop(len);
	}
}

void ylars::reactor_buf::clear()
{
	if (nullptr == _buf)
	{
		return;
	}
	buf_pool::instance()->release(_buf);
	_buf = nullptr;
}

int ylars::in_buf::readfd(int fd)
{
	// 获取读缓冲区数据长度
	int need_read = 0;
	if (ioctl(fd, FIONREAD, &need_read) < 0)
	{
		std::cerr << "ioctl FIONREAD error !" << std::endl;
		return -1;
	}
	auto pool = buf_pool::instance();
	if (nullptr == _buf)
	{
		_buf = pool->alloc_buf(need_read);
		if (nullptr == _buf)
		{
			std::cerr << "get memory error !" << std::endl;
			return -1;
		}
	}
	else
	{
		assert(_buf->head == 0);
		if (need_read + _buf->datalen > _buf->capacity)
		{
			// 剩余容量不足，重新申请
			auto new_buf = pool->alloc_buf(need_read + _buf->datalen);
			if (nullptr == _buf)
			{
				std::cerr << "get memory error !" << std::endl;
				return -1;
			}
			new_buf->copy(_buf);
			pool->release(_buf);
			_buf = new_buf;
		}
	}

	int already_read = 0;
	do
	{
		if (0 == need_read)
		{
			already_read = read(fd, _buf->data + _buf->datalen, _buf->capacity - _buf->datalen);
		}
		else
		{
			already_read = read(fd, _buf->data + _buf->datalen, need_read);
		}
	} while (-1 == already_read && EINTR == errno);
	if (already_read > 0)
	{
		if (0 != need_read)
		{
			assert(already_read == need_read);
		}
		_buf->datalen += already_read;
	}
	else if (already_read < 0 && (EAGAIN == errno || ECONNRESET == errno))
	{
		already_read = 0;
	}
	return already_read;
}

const char* ylars::in_buf::get_data()
{
	if (nullptr == _buf)
	{
		return nullptr;
	}
	return _buf->data + _buf->head;
}

void ylars::in_buf::adjust()
{
	if (nullptr == _buf)
	{
		return;
	}
	_buf->adjust();
}

int ylars::out_buf::send_data(const void* data, int datalen)
{
	if (nullptr == data || datalen == 0)
	{
		return -1;
	}
	auto pool = buf_pool::instance();
	if (nullptr == _buf)
	{
		_buf = pool->alloc_buf(datalen);
		if (nullptr == _buf)
		{
			std::cerr << "get memory error !" << std::endl;
			return -1;
		}
	}
	else
	{
		assert(_buf->head == 0);
		if (datalen + _buf->datalen > _buf->capacity)
		{
			// 超过最大容量，重新申请
			auto new_buf = pool->alloc_buf(datalen + _buf->datalen);
			if (nullptr == _buf)
			{
				std::cerr << "get memory error !" << std::endl;
				return -1;
			}
			new_buf->copy(_buf);
			pool->release(_buf);
			_buf = new_buf;
		}
	}
	memcpy(_buf->data + _buf->datalen, data, datalen);
	_buf->datalen += datalen;
	return 0;
}

int ylars::out_buf::write2fd(int fd)
{
	if (nullptr == _buf)
	{
		std::cerr << "_buf Memory is not initialized !" << std::endl;
		return -1;
	}
	int already_write = 0;
	do
	{
		already_write = write(fd, _buf->data, _buf->datalen);
	} while (-1 == already_write && EINTR == errno);
	if (already_write > 0)
	{
		_buf->pop(already_write);
		_buf->adjust();
	}
	else if (-1 == already_write && EAGAIN == errno)
	{
		// 并不是错误，而是非阻塞导致的-1
		already_write = 0;
	}
	return already_write;
}
