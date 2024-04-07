#ifndef _THREADPOOL_
#define _THREADPOOL_

// const int NUMBER = 2;
#define NUMBER 2

typedef struct Task Task;

typedef struct ThreadPool ThreadPool;

// 创建线程池并初始化
ThreadPool* createThreadPool(int min, int max, int queueSize);

int destoryThreadPool(ThreadPool* pool);

// 管理者线程任务函数
void* manager(void* arg);

// 工作线程任务函数
void* worker(void* arg);

void threadExit(ThreadPool* pool);

// 添加任务
void threadPoolAdd(ThreadPool* pool, void(*function)(void*), void* arg);

int threadPoolBusyNum(ThreadPool* pool);

int threadPoolLiveNum(ThreadPool* pool);

#endif