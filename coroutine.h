#pragma once
// 函数指针的头文件
#include <functional>
#include <stdint.h>
#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_SUSPEND 2
#define COROUTINE_RUNNING 3

// 函数包装器，包装的是返回值是void参数是空的函数
using coroutine_func = std::function<void()>;

class coroutine {
	struct cell_coroutine;	// 协程
	struct schedule;	// 调度器
	schedule* _sch;

public:
	coroutine();
	void init();	// 初始化
	static void _co_delete(cell_coroutine*);	// 销毁协程
	void* _co_new(coroutine_func);		// 创建协程需要传入回调函数执行任务
	int coroutine_new(coroutine_func);		// 将创建出的新协程加入调度器

	static void mainfunc(uintptr_t);	// 实现对任务的调用，uintptr是unsigned long的别名

	void coroutine_resume(int);		//恢复协程

	static void _save_stack(cell_coroutine*, char*);	// 保存栈空间的数据

	void coroutine_yeld();		//	协程让出（），指正在运行的
	int coroutine_status(int);	// 返回协程状态
	int coroutine_running();	//返回调度器中正在运行的id
	void destroy();

	virtual ~coroutine();



};

