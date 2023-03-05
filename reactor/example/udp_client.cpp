#include <udp_client.h>
#include <event_loop.h>
#include <iostream>
#include <strings.h>
#include <cstring>
#include <config_file.h>

void echo(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto cli = static_cast<ylars::udp_client*>(conn);
	char* buf = new char[msglen + 1];
	bzero(buf, msglen + 1);
	memcpy(buf, msg, msglen);
	std::cout << "msgid : " << msgid << std::endl
		<< "msglen : " << msglen << std::endl
		<< "msg : " << buf << std::endl;
	delete[] buf;
	cli->send_msg(1, "Bye", 4);
}


void bye(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	auto cli = static_cast<ylars::udp_client*>(conn);
	char* buf = new char[msglen + 1];
	bzero(buf, msglen + 1);
	memcpy(buf, msg, msglen);
	std::cout << "msgid : " << msgid << std::endl
		<< "msglen : " << msglen << std::endl
		<< "msg : " << buf << std::endl;
	delete[] buf;
	cli->disconnect();
}

int main()
{
	config_file::setPath("./conf/client.ini");
	auto conf = config_file::instance();
	auto ip = conf->GetString("server", "ip", "0.0.0.0");
	uint16_t port = conf->GetNumber("server", "port", 7777);
	ylars::event_loop loop{};
	ylars::udp_client cli{ &loop, ip.c_str(), port };
	cli.send_msg(1, "hello, I am client !", 21);
	cli.register_msgid(1, echo, nullptr);
	cli.register_msgid(2, bye, nullptr);
	loop.start_process();
	return 0;
}