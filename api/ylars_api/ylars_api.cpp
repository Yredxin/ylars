#include <ylars_reactor.h>
#include <ylars.pb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ylars_api.h"

ylars_api::ylars_api()
{
	// 创建udp套接字
	_sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
	if (_sockfd < 0)
	{
		perror("create udp socket error ");
		exit(1);
	}
}

ylars_api::~ylars_api()
{
	close(_sockfd);
}

int ylars_api::reg_init(uint32_t modid, uint32_t cmdid)
{
	ylars_route_t route;
	int	retry_cnt = 0;
	while (route.empty() && retry_cnt < 3)
	{
		get_route(modid, cmdid, route);
		if (route.empty() == true) {
			usleep(50000); //等待50ms
		}
		else {
			break;
		}
		++retry_cnt;
	}
	if (route.empty())
	{
		return ylars::RET_NOEXIST;
	}
	return ylars::RET_SUCC;
}

int ylars_api::get_host(uint32_t modid, uint32_t cmdid, std::string& ip, uint16_t& port)
{
	bzero(_tmp_buf, sizeof(_tmp_buf));

	// 每次报文的验证码
	static uint32_t check_seq = 0;

	// 获取校验码
	uint32_t seq = check_seq++;

	// 初始化服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)(9995 + ((modid + cmdid) % 3)));
	// 连接到本地
	inet_aton("127.0.0.1", &addr.sin_addr);

	// 连接服务器
	if (connect(_sockfd, (const sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("connect to server error ");
		return ylars::RET_SYSTEM_ERROR;
	}

	// 组织请求报文
	ylars::GetHostRequest req;
	req.set_modid(modid);
	req.set_cmdid(cmdid);
	req.set_seq(seq);
	ylars::msg_head head;
	head.msgid = ylars::ID_GetHostRequest;
	head.msglen = req.ByteSizeLong();
	memcpy(_tmp_buf, &head, MSG_HEAD_LEN);
	req.SerializeToArray(_tmp_buf + MSG_HEAD_LEN, head.msglen);

	// 发送数据
	if (sendto(_sockfd, _tmp_buf, head.msglen + MSG_HEAD_LEN, 0, nullptr, 0) < 0)
	{
		perror("send error ");
		return ylars::RET_SYSTEM_ERROR;
	}

	// 循环接收数据
	int readlen;
	ylars::GetHostResponse res;
	do
	{
		bzero(_tmp_buf, sizeof(_tmp_buf));
		if ((int)(readlen = recvfrom(_sockfd, _tmp_buf, sizeof(_tmp_buf), 0, nullptr, 0)) < 0)
		{
			perror("recv error ");
			return ylars::RET_SYSTEM_ERROR;
		}
		if (readlen < MSG_HEAD_LEN)
		{
			fprintf(stderr, "recv msg error\n");
			return ylars::RET_SYSTEM_ERROR;
		}
		memcpy(&head, _tmp_buf, MSG_HEAD_LEN);
		if (readlen != head.msglen + MSG_HEAD_LEN ||
			head.msgid != ylars::ID_GetHostResponse)
		{
			fprintf(stderr, "recv msg error\n");
			return ylars::RET_SYSTEM_ERROR;
		}
		if (!res.ParseFromArray(_tmp_buf + MSG_HEAD_LEN, head.msglen))
		{
			fprintf(stderr, "recv msg error\n");
			return ylars::RET_SYSTEM_ERROR;
		}
	} while (res.seq() < seq); // 当校验码大于时认为包丢失

	if (res.seq() != seq ||
		res.modid() != modid || res.cmdid() != cmdid)
	{
		// 错误包
		fprintf(stderr, "packet error\n");
		return ylars::RET_SYSTEM_ERROR;
	}

	if (res.retcode() != ylars::RET_SUCC)
	{
		return res.retcode();
	}
	ip = inet_ntoa(in_addr{ htonl((in_addr_t)res.host().ip()) });
	port = (uint16_t)res.host().port();

	return ylars::RET_SUCC;
}

void ylars_api::get_route(uint32_t modid, uint32_t cmdid, ylars_route_t& route)
{
	bzero(_tmp_buf, sizeof(_tmp_buf));

	// 初始化服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)(9995 + ((modid + cmdid) % 3)));
	// 连接到本地
	inet_aton("127.0.0.1", &addr.sin_addr);

	// 连接服务器
	if (connect(_sockfd, (const sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("connect to server error ");
		return;
	}

	// 组织请求报文
	ylars::GetRouteRequest req;
	req.set_modid(modid);
	req.set_cmdid(cmdid);
	ylars::msg_head head;
	head.msgid = ylars::ID_GetRouteRequest;
	head.msglen = req.ByteSizeLong();
	memcpy(_tmp_buf, &head, MSG_HEAD_LEN);
	req.SerializeToArray(_tmp_buf + MSG_HEAD_LEN, head.msglen);

	// 发送数据
	if (sendto(_sockfd, _tmp_buf, head.msglen + MSG_HEAD_LEN, 0, nullptr, 0) < 0)
	{
		perror("send error ");
		return;
	}

	// 循环接收数据
	int readlen;
	ylars::GetRouteResponse res;
	bzero(_tmp_buf, sizeof(_tmp_buf));
	if ((int)(readlen = recvfrom(_sockfd, _tmp_buf, sizeof(_tmp_buf), 0, nullptr, 0)) < 0)
	{
		perror("recv error ");
		return;
	}
	if (readlen < MSG_HEAD_LEN)
	{
		fprintf(stderr, "recv msg error\n");
		return;
	}
	memcpy(&head, _tmp_buf, MSG_HEAD_LEN);
	if (readlen != head.msglen + MSG_HEAD_LEN ||
		head.msgid != ylars::ID_GetRouteResponse)
	{
		fprintf(stderr, "recv msg error\n");
		return;
	}
	if (!res.ParseFromArray(_tmp_buf + MSG_HEAD_LEN, head.msglen))
	{
		fprintf(stderr, "recv msg error\n");
	}

	if (res.modid() != modid || res.cmdid() != cmdid)
	{
		// 错误包
		fprintf(stderr, "packet error\n");
		return;
	}

	std::string ip;
	uint32_t port;

	for (int i = 0; i < res.host_size(); i++)
	{
		auto& host = res.host(i);
		ip = inet_ntoa(in_addr{ htonl((in_addr_t)host.ip()) });
		port = (uint16_t)host.port();
		route.push_back(ylars_host_t{ ip, port });
	}
}

void ylars_api::report(uint32_t modid, uint32_t cmdid, std::string& ip, uint16_t port, bool is_succ)
{
	bzero(_tmp_buf, sizeof(_tmp_buf));

	// 初始化服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)(9995 + ((modid + cmdid) % 3)));
	// 连接到本地
	inet_aton("127.0.0.1", &addr.sin_addr);

	// 连接服务器
	if (connect(_sockfd, (const sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("connect to server error ");
	}

	// 组织请求报文
	ylars::ReportRequest req;
	req.set_modid(modid);
	req.set_cmdid(cmdid);
	req.set_issucc(is_succ);
	auto host = req.mutable_host();
	in_addr req_ip;
	inet_aton(ip.c_str(), &req_ip);
	host->set_ip(htonl(req_ip.s_addr));
	host->set_port(port);
	ylars::msg_head head;
	head.msgid = ylars::ID_ReportRequest;
	head.msglen = req.ByteSizeLong();
	memcpy(_tmp_buf, &head, MSG_HEAD_LEN);
	req.SerializeToArray(_tmp_buf + MSG_HEAD_LEN, head.msglen);

	// 发送数据
	if (sendto(_sockfd, _tmp_buf, head.msglen + MSG_HEAD_LEN, 0, nullptr, 0) < 0)
	{
		perror("send error ");
	}
}


