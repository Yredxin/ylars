#ifndef __YLARS_SUBSCRIBE__
#define __YLARS_SUBSCRIBE__
#include <pthread.h>
#include <map>
#include <set>
#include <vector>
#include <ylars_reactor.h>

typedef std::map<uint64_t, std::set<int>> sub_map_t;
typedef std::map<int, std::set<uint64_t>> pub_map_t;

class SubscribeList
{
public:
	static SubscribeList* instance();

	// 订阅
	void subscribe(uint64_t mod, int fd);

	// 取消订阅
	void unsubscribe(uint64_t mod, int fd);

	// 发布
	void publish(std::vector<uint64_t>& change_mods);

	// 分离线程需要的任务
	void make_publish_map(ylars::evn_set_t& fds, pub_map_t& need_publish);

private:
	// =========== 单例 ===========
	SubscribeList();
	~SubscribeList();
	static SubscribeList* _subscribe;
	static pthread_once_t _once;
	static void init();

	// ========== 业务 ==========
	// 订阅集合  业务模块 ==> 文件描述符集合
	sub_map_t _book_list;
	// 订阅锁
	pthread_mutex_t _book_mutex;

	// 发布集合  文件描述符  ==> 业务模块集合
	pub_map_t _push_list;
	// 发布锁
	pthread_mutex_t _push_mutex;

};

// 测试使用
void* publish_change_test(void* args);
// 查询服务器变化，主动推送
void* changet_publish(void* args);

#endif // !__YLARS_SUBSCRIBE__
