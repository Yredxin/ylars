#include <ylars.pb.h>
#include <ylars_reactor.h>

void report_test(ylars::conn_base* conn, void* agrs)
{
	// 测试消息
	ylars::ReportStatusReq req;
	req.set_modid(1);
	req.set_cmdid(1);
	req.set_caller(123);
	req.set_ts(time(nullptr));
	auto res = req.add_result();
	res->set_succ(5);
	res->set_err(1);
	res->set_ip(123123);
	res->set_port(7788);
	res->set_overload(true);
	res = req.add_result();
	res->set_succ(1);
	res->set_err(5);
	res->set_ip(11111);
	res->set_port(7788);
	res->set_overload(false);

	std::string req_str;
	req.SerializeToString(&req_str);
	conn->send_msg(ylars::ID_ReportStatusRequest, req_str.c_str(), req_str.length());
}

int main()
{
	ylars::event_loop loop;
	ylars::tcp_client cli{ &loop, "127.0.0.1", 7779 };

	cli.register_conn_start(report_test);

	loop.start_process();
	return 0;
}