#include "host_info.h"
#include "agent_main.h"

// 设置为overload
void host_info::set_overload()
{
	vsucc_cnt = 0;
	verr_cnt = lb_config.init_err_cnt;
	rsucc_cnt = 0;
	rerr_cnt = 0;

	contin_succ = 0;
	contin_err = 0;
	overload = true;

	overload_ts = time(nullptr);
}

// 设置为idle
void host_info::set_idle()
{
	vsucc_cnt = lb_config.init_succ_cnt;
	verr_cnt = 0;
	rsucc_cnt = 0;
	rerr_cnt = 0;

	contin_succ = 0;
	contin_err = 0;
	overload = false;
	idle_ts = time(nullptr);
}
