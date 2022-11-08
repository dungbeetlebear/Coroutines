#pragma once
// ����ָ���ͷ�ļ�
#include <functional>
#include <stdint.h>
#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_SUSPEND 2
#define COROUTINE_RUNNING 3

// ������װ������װ���Ƿ���ֵ��void�����ǿյĺ���
using coroutine_func = std::function<void()>;

class coroutine {
	struct cell_coroutine;	// Э��
	struct schedule;	// ������
	schedule* _sch;

public:
	coroutine();
	void init();	// ��ʼ��
	static void _co_delete(cell_coroutine*);	// ����Э��
	void* _co_new(coroutine_func);		// ����Э����Ҫ����ص�����ִ������
	int coroutine_new(coroutine_func);		// ������������Э�̼��������

	static void mainfunc(uintptr_t);	// ʵ�ֶ�����ĵ��ã�uintptr��unsigned long�ı���

	void coroutine_resume(int);		//�ָ�Э��

	static void _save_stack(cell_coroutine*, char*);	// ����ջ�ռ������

	void coroutine_yeld();		//	Э���ó�������ָ�������е�
	int coroutine_status(int);	// ����Э��״̬
	int coroutine_running();	//���ص��������������е�id
	void destroy();

	virtual ~coroutine();



};

