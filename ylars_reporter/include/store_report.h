#ifndef __YLARS_STORE_REPORT__
#define __YLARS_STORE_REPORT__
#include <mysql/mysql.h>
#include <ylars.pb.h>
#include <ylars_reactor.h>

typedef ylars::thread_queue<ylars::ReportStatusReq> store_queue_t;

class StoreReport
{
public:
	StoreReport();
	~StoreReport();

	// 将信息更新到数据库
	void report2db(ylars::ReportStatusReq& rep);
private:
	MYSQL _conn_db;
	char _sql[1024];
};

// 线程处理函数
void* store_main(void* agrs);

#endif // !__YLARS_STORE_REPORT__
