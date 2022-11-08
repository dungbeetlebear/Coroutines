#include "coroutine.h"
#include <iostream>
#include <stdlib.h>
#include <stddef.h>//ptrdiff_t
#include <ucontext.h>	// ��ǰ���̵���������Ϣ
#include <assert.h>
#include <string.h>

#define DEFAULT_COROUTINE 16
#define STACKSIZE (1024*1024)

struct coroutine::cell_coroutine {
	coroutine_func func;	// �ص�����
	ptrdiff_t cap;	// ��¼��ǰЭ��ջ�ռ����ֵ
	ptrdiff_t size; // ��ǰЭ��ʵ��ʹ�ÿռ�
	int status;		//��¼��ǰЭ�̵�״̬����������������������ʹ�ú��ʾ
	char* stack;	// ջ�ռ�
	ucontext_t ctx;	// ucontext�ṹ�������ʾЭ������ʱ����������Ϣ����������ʱ�ļĴ�������ջָ��ȣ�
};

struct coroutine::schedule {
	char stack[STACKSIZE];
	int cap;	// ��¼��ǰ�������ܴ�ŵ��������
	int nco;	// ʵ�ʹ����Э������
	int  running;	// �������е�Э�̺�
	cell_coroutine** co;	// �����ЧЭ��
	ucontext_t main;	// �����������"����"�����ݴ�Э���л�ʱ�洢����Ϣ

};

// �����̵����ݱ��浽Э�̵�ջ�ռ�
// ջ�ռ��ջ��ʱ�͵�ַ��ջ���Ǹߵ�ַ
void coroutine::_save_stack(cell_coroutine* c, char* buttom)
{
	char dummy = 0;	// ���ﴫ���top����������׵�ַ����ʾջ��ָ��
	// ���ڶ�ջ������˳���Ǻ��ڴ��ַ������˳���෴������Ϊdummy����󴴽��ı������������ַָ��ջ��
	// ����ջ�ף��ڴ�ߵ�ַ��-ջ�����ڴ�͵�ַ��=ջ��ʹ�ÿռ�
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
	// ��������ص�������Э�������˳�����ռ��ϵͳ��Դ
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
		c->ctx.uc_stack.ss_sp = _sch->stack;			// ջ�ռ�ָ��Ϊ��������ջ�ռ�
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
		// ����ǰЭ�̵�ջ�ռ俽������������ջ�ռ�
		memcpy(_sch->stack + STACKSIZE - c->size, c->stack, c->size);
		c->status = COROUTINE_RUNNING;
		_sch->running = id;
		swapcontext(&_sch->main, &c->ctx);
		break;
	}
	}
}


