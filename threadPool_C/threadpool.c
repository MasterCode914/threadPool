#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 定义任务队列
struct Task
{
    void (*function)(void* arg);        // 函数指针
    void* arg;                          // 函数参数
};


// 线程池
struct ThreadPool
{
    Task* taskQ;                // 堆空间
    int queueCapacity;          // 队列最大长度
    int queueSize;              // 当前队列长度
    int queueFront;             // 队头->取出
    int queueRrear;             // 队尾->插入

    pthread_t manangerID;           // 管理者线程

    pthread_t* threadIDs;           // 工作线程
    int maxNum;                     // 最大工作线程数
    int minNum;                     // 最小工作线程数
    int liveNum;                    // 存活的工作线程数
    int busyNum;                    // 正在工作的工作线程数
    int exitNum;                    // 要销毁的工作线程数

    pthread_mutex_t mutexPool;          // 线程池互斥锁
    pthread_mutex_t mutexBusy;          // busyNum操作频繁，单独使用一个互斥锁

    // 条件变量->阻塞生产者和消费者线程
    pthread_cond_t condHasData;               // 任务队列不空
    pthread_cond_t condHasCapacity;           // 任务队列不满

    int shutDown;                           // 是否销毁线程池   1: 销毁  0: 存货
};


// 创建线程池并初始化
ThreadPool* createThreadPool(int min, int max, int queueCapacity)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do
    {
        if (pool == NULL) {
            printf("malloc pool  fail..\n");
            break;
        }

        pool->taskQ = (Task*)malloc(sizeof(Task) * queueCapacity);
        if (pool->taskQ == NULL) {
            printf("malloc task queue fail..\n");
            break;
        }
        memset(pool->taskQ, 0, sizeof(Task)*queueCapacity);
        pool->queueCapacity = queueCapacity;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRrear = 0;

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL) {
            printf("malloc work threads fail..\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->maxNum = max;
        pool->minNum = min;
        pool->liveNum = min;
        pool->busyNum = 0;
        pool->exitNum = 0;
        
        // 初始化互斥锁和条件变量
        if (pthread_mutex_init(&pool->mutexPool, NULL) 
        || pthread_mutex_init(&pool->mutexBusy, NULL)
        || pthread_cond_init(&pool->condHasData, NULL)
        || pthread_mutex_init(&pool->condHasCapacity, NULL)) {
            printf("init mutex/cond fail..\n");
            break;
        }
        
        pool->shutDown = 0;

        // 工作线程/管理者线程会修改先乘除，需要传递线程池参数
        pthread_create(&pool->manangerID, NULL, manager, pool);

        for (int i = 0; i < min; i++) {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);    
        }
        return pool;
    } while (0);

    // 如果创建不成功释放资源
    if (pool && pool->taskQ)    free(pool->taskQ);
    if (pool && pool->threadIDs)    free(pool->threadIDs);
    if (pool)   free(pool);

    return NULL;
}


int destoryThreadPool(ThreadPool* pool) 
{
    if (pool == NULL)   return -1;

    pool->shutDown = 1;             // 关闭线程池

    // 主线程回收管理者和工作线程
    pthread_join(pool->manangerID, NULL);       // 阻塞回收管理者线程

    // 唤醒阻塞的工作线程
    // 工作线程执行到shutdown=1会进行回收
    for (int i = 0; i < pool->liveNum; i++) {
        pthread_cond_signal(&pool->condHasData);
    }

    // 回收堆空间
    if (pool->taskQ)    free(pool->taskQ);
    if (pool->threadIDs)    free(pool->threadIDs);

    // 销毁互斥锁和条件变量
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->condHasData);
    pthread_cond_destroy(&pool->condHasCapacity);

    free(pool);
    pool = NULL;

    return 0;

}

void threadExit(ThreadPool* pool)
{
    pthread_t pid = pthread_self();
    for (int i = 0; i < pool->liveNum; i++) {
        if (pool->threadIDs[i] == pid) {
            printf("thread %ld exiting..\n", pid);
            pool->threadIDs[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}


void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    
    // 没有销毁就一直执行
    while (1) {
        pthread_mutex_lock(&pool->mutexPool);
        while (pool->queueSize == 0 && !pool->shutDown) {
            // 任务队列未空阻塞
            pthread_cond_wait(&pool->condHasData, &pool->mutexPool);

            // 判断是否销毁线程
            if (pool->exitNum != 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);       // 该工作线程直接结束了，不用return了
                    // 这两行不应该换一下位置么？
                }
            }
        }

        // 自动销毁
        if (pool->shutDown) {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        // 从任务队列中取出任务执行
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 移动头节点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        // 唤醒一个由于队列满而阻塞的线程
        pthread_cond_signal(&pool->condHasCapacity);
        pthread_mutex_unlock(&pool->mutexPool);

        // 修改工作线程busy数量
        printf("thread %ld start working..\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        // 执行任务
        task.function(task.arg);
        free(task.arg);                 // 指针多用这两行没事
        task.arg = NULL;

        printf("thread %ld end working..\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }

    return NULL;
}


void* manager(void* arg) 
{
    ThreadPool* pool = (ThreadPool*)arg;

    while (!pool->shutDown) {
        // 每隔3s检查一次
        sleep(3);

        // 取出线程池存活的线程数和当前工作的线程数量
        pthread_mutex_lock(&pool->mutexPool);
        int liveNum = pool->liveNum;
        int queueSize = pool->queueSize;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 添加线程
        if (busyNum < queueSize && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER; i++) {
                if (pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        // 销毁线程
        // 忙的线程 * 2 < 存活的线程 && 存活的线程 > 最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            
            // 让工作的线程自杀
            for (int i = 0; i < NUMBER; i++) {
                pthread_cond_signal(&pool->condHasData);
            }
        }
    }
}

void threadPoolAdd(ThreadPool* pool, void(*function)(void*), void* arg)
{
    if (pool == NULL)    return;
    pthread_mutex_lock(&pool->mutexPool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutDown) {
        pthread_cond_wait(&pool->condHasCapacity, &pool->mutexPool);

    }

    if (pool->shutDown)
        return ;
    pool->taskQ[pool->queueRrear].function = function;
    pool->taskQ[pool->queueRrear].arg = arg;
    pool->queueRrear = (pool->queueRrear + 1) % pool->queueCapacity;
    pool->queueSize++;


    pthread_cond_signal(&pool->condHasData);
    pthread_mutex_unlock(&pool->mutexPool);

}

int threadPoolBusyNum(ThreadPool* pool) 
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    
    return busyNum;
}

int threadPoolLiveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);

    return liveNum;
}