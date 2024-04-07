#ifndef _TASKQUEUE_
#define _TASKQUEUE_

#include <pthread.h>
#include <queue>

using callback = void(*)(void*);        // 别名->函数指针

template <typename T>
struct Task
{
    // 直接初始化
    Task()
    {
        function = nullptr;
        arg = nullptr;
    }

    Task(callback f, void* arg)
    {
        function = f;
        this->arg = (T*)arg;
    }
    callback function;        // 函数指针
    T* arg;                          // 函数参数
};

template <typename T>
class TaskQueue
{
public:
    TaskQueue();
    ~TaskQueue();

    // 添加任务
    void addTask(Task<T> t);
    void addTask(callback f, void* arg);

    // 取出任务
    Task<T> takeTask();

    // 队列大小
    inline int getTaskNumber()          // 内联函数->减少函数调用栈
    {
        return m_queue.size();
    }

private:
    pthread_mutex_t m_mutex;        // 任务队列互斥锁
    std::queue<Task<T>> m_queue;       // 任务队列    
    
};

#endif