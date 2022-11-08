#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include "coroutine.h"

void task1(coroutine* c, int num) {
    for (int i = 0; i < 5; i++) {
        std::cout << "coroutine:" << c->coroutine_running() << " " << num + i << "\n";
        c->coroutine_yeld();
    }
}

void task2(coroutine* c, const char* msg) {
    for (int i = 0; i < 5; i++) {
        std::cout << "coroutine:" << c->coroutine_running()<<" " << msg + std::to_string(i)<< "\n";
        c->coroutine_yeld();
    }
}

void test(coroutine* c) {
    int co1 = c->coroutine_new(std::bind(task1, c, 5));
    int co2 = c->coroutine_new(std::bind(task2, c, "china"));
    while (c->coroutine_status(co1) && c->coroutine_status(co2)) {
        c->coroutine_resume(co1);
        c->coroutine_resume(co2);
    }
}
int main()
{
    coroutine c;
    c.init();
    test(&c);
}