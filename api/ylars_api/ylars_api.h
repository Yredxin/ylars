#ifndef __YLARS_CPP_API__
#define __YLARS_CPP_API__
#include <stdint.h>
#include <string>
#include <vector>

typedef std::pair<std::string, uint32_t> ylars_host_t;
typedef std::vector<ylars_host_t> ylars_route_t;

class ylars_api
{
public:
	ylars_api();
	~ylars_api();
	// 初始化，一个模块调用一次
	int reg_init(uint32_t modid, uint32_t cmdid);
	// 获取一个主机，成功与否需要调用report上报
	int get_host(uint32_t modid, uint32_t cmdid, std::string& ip, uint16_t& port);
	// 获取modid和cmdid下所有主机
	void get_route(uint32_t modid, uint32_t cmdid, ylars_route_t& route);
	// 上报是否成功
	void report(uint32_t modid, uint32_t cmdid, std::string& ip, uint16_t port, bool is_succ);

private:
	int _sockfd;
	char _tmp_buf[1024];
};

#endif // !__YLARS_CPP_API__
