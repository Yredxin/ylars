#include <iostream>
#include "message.h"

void ylars::msg_router::register_router(int msgid, router_callback cb, void* args)
{
	if (_router.find(msgid) != _router.end())
	{
		std::cerr << "msgid already registered !" << std::endl;
		return;
	}
	if (nullptr == cb)
	{
		std::cerr << "register fail !" << std::endl;
		return;
	}
	_router[msgid] = cb;
	_args[msgid] = args;
}

void ylars::msg_router::call(conn_base* conn, int msgid, const void* msg, int msglen)
{
	if (_router.find(msgid) == _router.end())
	{
		std::cerr << "msgid not registered yet !" << std::endl;
		return;
	}
	_router[msgid](conn, msgid, msg, msglen, _args[msgid]);
}
