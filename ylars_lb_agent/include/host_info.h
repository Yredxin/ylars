#ifndef __YLARS_HOST_INFO__
#define __YLARS_HOST_INFO__
#include <stdint.h>
#include <ctime>
struct host_info
{
	host_info(uint32_t p_ip, uint32_t p_port, int init_succ) :
		ip{ p_ip },
		port{ p_port },
		vsucc_cnt{ init_succ },
		rsucc_cnt{ 0 },
		contin_succ{ 0 },
		verr_cnt{ 0 },
		rerr_cnt{ 0 },
		contin_err{ 0 },
		overload{ false }
	{
		idle_ts = time(nullptr);
	}

	// 设置为overload
	void set_overload();
	// 设置为idle
	void set_idle();

	uint32_t ip;		// 模块功能ip
	uint32_t port;		// 模块功能端口
	int vsucc_cnt;		// 虚拟成功次数，在负载均衡算法中防止开始出现失败导致的失败率过高问题
	int	rsucc_cnt;		// 真实成功次数
	int contin_succ;	// 连续成功次数
	int verr_cnt;		// 虚拟失败次数，防止在首次调用负载容器时成功率过高问题
	int rerr_cnt;		// 真实失败次数
	int contin_err;		// 连续失败次数
	bool overload;		// 是否负载

	long idle_ts;		// 当主机变为idle时的时间
	long overload_ts;	// 当主机变为overload的时间
};

#endif // !__YLARS_HOST_INFO__
