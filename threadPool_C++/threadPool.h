#ifndef _THREADPOOL_
#define _THREADPOOL_
#include <pthread.h>
#include <iostream>
#include <queue>
#include <string.h>
#include <unistd.h>
#include "taskQueue.h"
#include "taskQueue.cpp"

#define NUMBER 2

// 线程池
template <typename T>
class ThreadPool 
{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    int getBusyNum();
    int getLiveNum();
    void addTask(Task<T> task);

private: 
    static void* manager(void*);
    static void* worker(void*);
    void threadExit();

private:
    TaskQueue<T>* taskQ;  
    pthread_t manangerID;           // 管理者线程
    pthread_t* threadIDs;           // 工作线程
    int maxNum;                     // 最大工作线程数
    int minNum;                     // 最小工作线程数
    int liveNum;                    // 存活的工作线程数
    int busyNum;                    // 正在工作的工作线程数
    int exitNum;                    // 要销毁的工作线程数

    pthread_mutex_t mutexPool;          // 线程池互斥锁
    pthread_cond_t condHasData;         // 任务队列不空

    bool shutDown = false;              // 是否销毁线程池..true: 销毁  false: 存货
};


#endif