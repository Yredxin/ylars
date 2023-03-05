#include "store_report.h"

StoreReport::StoreReport()
{
	// 初始化数据库
	mysql_init(&_conn_db);

	// 设置数据库超时重连
	mysql_options(&_conn_db, MYSQL_OPT_CONNECT_TIMEOUT, "30");
	int reconn = 1;
	mysql_options(&_conn_db, MYSQL_OPT_RECONNECT, &reconn);

	// 获取数据库配置文件
	auto conf = config_file::instance();
	auto ip = conf->GetString("mysql", "db_ip", "0.0.0.0");
	auto port = conf->GetNumber("mysql", "db_port", 3306);
	auto user = conf->GetString("mysql", "db_user", "root");
	auto passwd = conf->GetString("mysql", "db_passwd", "");
	auto db_name = conf->GetString("mysql", "db_name", "ylars_dns");

	// 连接数据库
	if (!mysql_real_connect(&_conn_db, ip.c_str(), user.c_str(),
		passwd.c_str(), db_name.c_str(), port, nullptr, 0))
	{
		fprintf(stderr, "mysql connect error, error is %s", mysql_error(&_conn_db));
		exit(1);
	}
}

StoreReport::~StoreReport()
{
	mysql_close(&_conn_db);
}

void StoreReport::report2db(ylars::ReportStatusReq& rep)
{
	for (int i = 0; i < rep.result_size(); i++)
	{
		memset(_sql, 0, sizeof(_sql));
		// 获取主机信息
		const auto& res = rep.result(i);

		// 拼接sql语句
		sprintf(_sql, "INSERT INTO ServerCallStatus"
			"(modid, cmdid, ip, port, caller, succ_cnt, err_cnt, ts, overload) "
			"VALUES (%u, %u, %u, %u, %d, %u, %u, %u, %d) ON DUPLICATE KEY "
			"UPDATE succ_cnt = %u, err_cnt = %u, ts = %u, overload = %u",
			rep.modid(), rep.cmdid(), res.ip(), res.port(), rep.caller(),
			res.succ(), res.err(), rep.ts(), res.overload(),
			res.succ(), res.err(), rep.ts(), res.overload());

		// 执行sql语句
		if (mysql_real_query(&_conn_db, _sql, strlen(_sql)) != 0)
		{
			// 失败
			fprintf(stderr, "mysql query error, error is %s\n", mysql_error(&_conn_db));
		}
	}
}

