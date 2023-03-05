#include <buf_pool.h>
#include <io_buf.h>
#include <iostream>
#include <cstring>

int main()
{
#if 0
	auto pool = ylars::buf_pool::instance();
	auto buf = pool->alloc_buf(10);
	auto buf2 = pool->alloc_buf(5);
	memcpy(buf->data, "helloasd", 10);
	buf2->copy(buf);
	buf->pop(2);
	buf->adjust();
	std::cout << "buf1:" << buf->capacity << ":" << buf->datalen << ":" << buf->data + buf->head << std::endl;
	std::cout << "buf2:" << buf2->capacity << ":" << buf2->datalen << ":" << buf2->data + buf->head << std::endl;
#endif
	auto pool = ylars::buf_pool::instance();
	auto buf = pool->alloc_buf(10);
	memcpy(buf->data, "helloasd", 10);
	pool->release(buf);
	buf = nullptr;
	buf = pool->alloc_buf(20);
	std::cout << "buf1:" << buf->capacity << ":" << buf->datalen << ":" << buf->data + buf->head << std::endl;
}