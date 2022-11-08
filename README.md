# Coroutines
使用Linux提供的ucontext库，围绕ucontext_t结构体和XXXcontext()封装一个协程库，目的在于解决高并发时的线程创建销毁、线程锁每次转换造成的性能开销。

有机会再研究.md文件的的功能，先作简单记事本用

协程的优缺点这里先不提，简单百度即可有答案，这里介绍下整个一个小项目的结构以及必要的解释

0.介绍
这里使用到了ucontext_t结构体：整个项目都是以它为核心，其意义是程序运行时的上下文信息（寄存器、堆栈指针等）
还有围绕这个结构体的操作函数：
int getcontext(ucontext_t * ucp);
void makecontext(ucontext_t *ucp, void(*func)(), int argc, ...);
int setcontext(const ucontext_t *ucp)；
int swapcontext(ucontext_t *oucp, ucontext_t *ucp)；

1.名词说明
协程调度器 代码中的schedule
用户协程 代码中的cell_coroutine
栈空间/栈区 对应schedule中的成员stack
栈缓存/缓存区 对应的是cell_coroutine中的成员stack
主协程上下文 schedule中的main, 对应main函数中的上下文
用户协程上下文 cell_coroutine中的ctx, 对应每个用户协程自身的上下文
当前协程的状态 对应cell_coroutine中的status，记录当前协程的状态，死亡、就绪、挂起、运行使用宏表示
栈空间的总容量、可用容量使用cap、size表示
调度器存放的当前、最大协程实例数量 nco、cap

3.基本思路：封装一个协程类，有两个结构体成员，分别是协程调度器、用户协程。构造函数调用初始化函数实例化调度器，
          成员函数有销毁销毁协程(回收用户携程自身以及栈成员的其堆区空间)；
          创建用户携程传入回调函数并放入类维护的协程容器里面（这里并没有使用标准模板库，而是用二维指针数组，因为操作并不复杂）
          供makecontext传入的回调函数，内容是调用用户传入的回调后立即对调用完成的用户携程做销毁操作，回调中调
          用户协程恢复，重点使用XXXcontext()函数实现用户级的线程切换，将传入的协程id对应的协程实例从就绪或者挂起态转换到running态
          这中间涉及到上下文的交换和栈区的互相拷贝，这里相当于保护现场和恢复现场了
          用户协程挂起，所做的事和恢复大致相反
          值得注意的是保存栈空间的函数_save_stack，变量dummy作为当前程序创建得到最后一个变量，一定是创建在栈区的最顶端（内存最低的地址），
          传入参数是调度器的stack头指针，表示栈底指针，相减可得即将要保栈容量，之后保存到对应的用户携程实例里面并收藏起来
          
          
 main函数简单使用该对象执行循环输出任务，有机会使用到高并发的服务器中并和线程池进行性能比较
整个库内容不多，整体封装到一个类里面，回调函数需要用户自己包装并传入创建好的实例中结合yeld和resume挂起和恢复协程
          
