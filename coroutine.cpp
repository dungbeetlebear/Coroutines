#include "coroutine.h"
#include <iostream>
#include <stdlib.h>
#include <stddef.h>//ptrdiff_t
#include <ucontext.h>	// 当前进程的上下文信息
#include <assert.h>
#include <string.h>

#define DEFAULT_COROUTINE 16
#define STACKSIZE (1024*1024)

struct coroutine::cell_coroutine {
	coroutine_func func;	// 回调函数
	ptrdiff_t cap;	// 记录当前协程栈空间最大值
	ptrdiff_t size; // 当前协程实际使用空间
	int status;		//记录当前协程的状态，死亡、就绪、挂起、运行使用宏表示
	char* stack;	// 栈空间
	ucontext_t ctx;	// ucontext结构体变量表示协程运行时的上下文信息（程序运行时的寄存器、堆栈指针等）
};

struct coroutine::schedule {
	char stack[STACKSIZE];
	int cap;	// 记录当前调度器能存放的最大数量
	int nco;	// 实际管理的协程数量
	int  running;	// 正在运行的协程号
	cell_coroutine** co;	// 存放有效协程
	ucontext_t main;	// 这里的上下文"盒子"用于暂存协程切换时存储的信息

};

// 将进程的数据保存到协程的栈空间
// 栈空间的栈顶时低地址，栈底是高地址
void coroutine::_save_stack(cell_coroutine* c, char* buttom)
{
	char dummy = 0;	// 这里传入的top就是数组的首地址，表示栈顶指针
	// 由于堆栈的增长顺序是和内存地址增长的顺序相反，又因为dummy是最后创建的变量，所以其地址指向栈顶
	// 所以栈底（内存高地址）-栈顶（内存低地址）=栈的使用空间
	assert(buttom - &dummy <= STACKSIZE);
	if (c->cap < buttom - &dummy) {
		free(c->stack);
		c->cap = buttom - &dummy;
		c->stack = (char*)malloc(c->cap);
	}
	c->size = buttom - &dummy;
	memcpy(c->stack, &dummy, c->size);

}

void coroutine::coroutine_yeld()
{
	int id = _sch->running;
	assert(id >= 0 && id <= _sch->cap);
	cell_coroutine* c = _sch->co[id];
	assert((char*)&c > _sch->stack);
	_save_stack(c, _sch->stack + STACKSIZE);
	c->status = COROUTINE_SUSPEND;
	_sch->running = -1;
	swapcontext(&c->ctx, &_sch->main);
}

int coroutine::coroutine_status(int id)
{
	if (!_sch->co[id]) {
		return COROUTINE_DEAD;
	}
	return _sch->co[id]->status;
}

int coroutine::coroutine_running()
{
	return _sch->running;
}

void coroutine::destroy()
{
	if (_sch) {
		if (_sch->co) {
			for (int i = 0; i < _sch->cap; i++) {
				if (_sch->co[i]) {
					free(_sch->co[i]);
					_sch->co = nullptr;
				}
			}
			free(_sch->co);
			_sch->co = nullptr;
		}
		free(_sch);
		_sch = nullptr;
	}
}

coroutine::~coroutine()
{
	destroy();
	std::cout << "coroutine is destroied!\n";

}



coroutine::coroutine() :_sch(nullptr)
{

}

void coroutine::init()
{
	if (!_sch) {
		_sch = new schedule;
		if (!_sch) {
			assert(0);
			return;
		}
		_sch->cap = DEFAULT_COROUTINE;
		_sch->nco = 0;
		_sch->running = -1;
		memset(_sch->stack, 0, STACKSIZE);
		_sch->main = {};
		_sch->co = new cell_coroutine * [_sch->cap];

	}
}

void coroutine::_co_delete(cell_coroutine* c)
{
	if (c) {
		if (c->stack) {
			free(c->stack);
			c->stack = nullptr;
		}
		free(c);
		c = nullptr;
	}
}


void* coroutine::_co_new(coroutine_func func)
{
	cell_coroutine* c = new cell_coroutine;
	c->cap = 0;
	c->size = 0;
	c->func = func;
	c->status = COROUTINE_READY;
	c->ctx = {};
	return c;
}

int coroutine::coroutine_new(coroutine_func func)
{
	cell_coroutine* c = (cell_coroutine*)_co_new(func);
	if (_sch->nco >= _sch->cap) {
		int id = _sch->cap;
		_sch->co = (cell_coroutine**)realloc(_sch->co, sizeof(cell_coroutine*) * 2 * _sch->cap);
		_sch->co[id] = c;
		++_sch->nco;
		_sch->cap *= 2;
		return id;
	}
	for (int i = 0; i < _sch->cap; i++) {
		int id = (_sch->nco + i) % _sch->cap;
		if (!_sch->co[id]) {
			_sch->co[id] = c;
			_sch->nco++;
			return id;
		}
	}
}

void coroutine::mainfunc(uintptr_t ptr)
{
	schedule* s = (schedule*)ptr;
	int id = s->running;
	cell_coroutine* c = s->co[id];
	// 当调用完回调函数后，协程立刻退出不会占用系统资源
	c->func();
	_co_delete(c);
	s->co[id] = nullptr;
	s->running = -1;
	s->nco--;
}

void coroutine::coroutine_resume(int id)
{
	assert(_sch->running == -1);
	assert(id >= 0 && id <= _sch->cap);
	cell_coroutine* c = _sch->co[id];
	int status = c->status;
	switch (status) {
	case COROUTINE_READY: {
		getcontext(&c->ctx);
		c->ctx.uc_stack.ss_sp = _sch->stack;			// 栈空间指定为调度器的栈空间
		c->ctx.uc_stack.ss_size = STACKSIZE;
		c->ctx.uc_link = &_sch->main;
		c->status = COROUTINE_RUNNING;
		_sch->running = id;
		uintptr_t ptr = (uintptr_t)_sch;
		makecontext(&c->ctx, (void(*)())mainfunc, 1, ptr);
		swapcontext(&_sch->main, &c->ctx);
		break;
	}
	case COROUTINE_SUSPEND: {
		// 将当前协程的栈空间拷贝到调度器的栈空间
		memcpy(_sch->stack + STACKSIZE - c->size, c->stack, c->size);
		c->status = COROUTINE_RUNNING;
		_sch->running = id;
		swapcontext(&_sch->main, &c->ctx);
		break;
	}
	}
}


