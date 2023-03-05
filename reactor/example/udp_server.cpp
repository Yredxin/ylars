#include <udp_server.h>
#include <event_loop.h>
#include <iostream>
#include <strings.h>
#include <cstring>
#include <config_file.h>

void echo(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto ser = static_cast<ylars::udp_server*>(conn);
	char* buf = new char[msglen + 1];
	bzero(buf, msglen + 1);
	memcpy(buf, msg, msglen);
	std::cout << "msgid : " << msgid << std::endl
		<< "msglen : " << msglen << std::endl
		<< "msg : " << buf << std::endl;
	delete[] buf;
	ser->send_msg(2, msg, msglen);
}


int main()
{
	config_file::setPath("./conf/reactor.ini");
	auto conf = config_file::instance();
	auto ip = conf->GetString("reactor", "ip", "0.0.0.0");
	uint16_t port = conf->GetNumber("reactor", "port", 7777);
	ylars::event_loop loop{};
	ylars::udp_server ser{ &loop, ip.c_str(), port };
	ser.register_msgid(1, echo, nullptr);
	loop.start_process();
	return 0;
}
