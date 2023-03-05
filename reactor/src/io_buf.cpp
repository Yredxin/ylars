#include <iostream>
#include <strings.h>
#include <cstring>
#include "io_buf.h"

ylars::io_buf::io_buf(int capacity) :
	capacity{ capacity },
	datalen{ 0 },
	data{ new char[capacity] },
	head{ 0 },
	next{ nullptr }
{
	if (nullptr == data)
	{
		std::cerr << "Failed to get heap memory !" << std::endl;
		exit(1);
	}
	//pthread_mutex_init(&_mem_mutex, nullptr);
}

ylars::io_buf::io_buf(const io_buf& buf)
{
	if (this->capacity >= buf.datalen)
	{
		//pthread_mutex_lock(&_mem_mutex);
		this->datalen = buf.datalen;
		memcpy(data, buf.data, buf.datalen);
		//pthread_mutex_unlock(&_mem_mutex);
	}
}

ylars::io_buf& ylars::io_buf::operator=(const io_buf& buf)
{
	// TODO: 在此处插入 return 语句
	if (this->capacity >= buf.datalen)
	{
		//pthread_mutex_lock(&_mem_mutex);
		this->datalen = buf.datalen;
		memcpy(data, buf.data, buf.datalen);
		//pthread_mutex_unlock(&_mem_mutex);
	}
	return *this;
}

ylars::io_buf::~io_buf()
{
	if (nullptr != data)
	{
		delete[]data;
	}
	//pthread_mutex_destroy(&_mem_mutex);
}

void ylars::io_buf::pop(int n)
{
	if (n < 0)
	{
		return;
	}
	if (n > datalen)
	{
		n = datalen;
	}
	//pthread_mutex_lock(&_mem_mutex);
	head += n;
	datalen -= n;
	//pthread_mutex_unlock(&_mem_mutex);
}

void ylars::io_buf::adjust()
{
	if (head > 0)
	{
		//pthread_mutex_lock(&_mem_mutex);
		if (datalen > 0)
		{
			memmove(data, data + head, datalen);
		}
		head = 0;
		bzero(data + datalen, capacity - datalen);
		//pthread_mutex_unlock(&_mem_mutex);
	}
}

void ylars::io_buf::copy(const io_buf* buf)
{
	if (nullptr != buf && buf->datalen >= buf->datalen && this->capacity >= buf->datalen)
	{
		// 抛出之前所有数据
		pop(datalen);
		adjust();
		// 拷贝
		//pthread_mutex_lock(&_mem_mutex);
		this->datalen = buf->datalen;
		memcpy(data, buf->data + buf->head, buf->datalen);
		//pthread_mutex_unlock(&_mem_mutex);
	}
}
