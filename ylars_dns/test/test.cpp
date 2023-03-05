#include <ylars_reactor.h>
#include <ylars.pb.h>
#include <arpa/inet.h>

void conn_start(ylars::conn_base* conn, void* agrs)
{
	ylars::GetRouteRequest req;
	req.set_modid(1);
	req.set_cmdid(2);
	std::string req_str;
	if (req.SerializeToString(&req_str))
	{
		conn->send_msg(ylars::ID_GetRouteRequest, req_str.c_str(), req_str.length());
	}
}

void get_res(ylars::conn_base* conn, int msgid, const void* msg, int msglen, void* args)
{
	if (strncmp("error", static_cast<const char*>(msg), msglen) == 0)
	{
		std::cout << "server error !" << std::endl;
	}
	ylars::GetRouteResponse res;
	res.ParseFromArray(msg, msglen);
	auto hosts = res.host();
	char ipbuf[17] = { 0 };
	struct in_addr addr;
	for (int i = 0; i < hosts.size(); i++)
	{
		addr.s_addr = htonl(hosts[i].ip());
		printf("ip = %s -- port = %d\n", inet_ntop(AF_INET, &addr, ipbuf, 16), hosts[i].port());
	}
}

int main(int argc, char** argv)
{
	ylars::event_loop loop;
	ylars::tcp_client cli{ &loop, "127.0.0.1", 7778 };

	cli.register_conn_start(conn_start);
	cli.register_msgid(ylars::ID_GetRouteResponse, get_res);

	loop.start_process();
}