#include "store_report.h"

typedef std::pair<StoreReport*, store_queue_t*> thread_report_t;

void deal_report(ylars::event_loop* loop, int fd, void* args)
{
	auto store_pair = static_cast<thread_report_t*>(args);
	std::queue<ylars::ReportStatusReq> tasks;

	// 获取当前的任务集合
	store_pair->second->recv_tasks(tasks);

	// 处理任务
	while (!tasks.empty())
	{
		auto& rep = tasks.front();
		store_pair->first->report2db(rep);
		tasks.pop();
	}
}

void* store_main(void* args)
{
	auto task_queue = static_cast<store_queue_t*>(args);
	ylars::event_loop loop;
	task_queue->set_event_loop(&loop);
	StoreReport srt;

	thread_report_t store_pair(&srt, task_queue);
	task_queue->set_task_cb(deal_report, &store_pair);

	loop.start_process();
	return nullptr;
}