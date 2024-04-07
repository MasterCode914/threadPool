#include "threadPool.h"
#include <string>

template <typename T>
ThreadPool<T>::ThreadPool(int min, int max)
{
    do
    {
        this->taskQ = new TaskQueue<T>;
        if (this->taskQ == nullptr) {
            std::cout << "malloc task queue fail.." << std::endl;
            break;
        }

        threadIDs = new pthread_t[maxNum];
        if (threadIDs == nullptr) {
            std::cout << "malloc work threads fail.." << std::endl;
            break;
        }
        memset(threadIDs, 0, sizeof(pthread_t) * max);
        maxNum = max;
        minNum = min;
        liveNum = min;
        busyNum = 0;
        exitNum = 0;
        
        // 初始化互斥锁和条件变量
        if (pthread_mutex_init(&mutexPool, NULL) 
        || pthread_cond_init(&condHasData, NULL)) {
            std::cout << "init mutex/cond fail.." << std::endl;
            break;
        }
    
        // 工作线程/管理者线程会修改先乘除，需要传递线程池参数
        // 第三个参数需要是一个有效的地址，非静态成员函数在类未实例化之前没有地址
        // 第四个参数也需要是一个实例化，传递pool并不是实例化的，this可以表示当前对象
        // 静态方法只能访问静态成员变量或函数。只能通过this访问非静态成员函数和变量
        pthread_create(&manangerID, NULL, manager, this);   

        for (int i = 0; i < min; i++) {
            pthread_create(&threadIDs[i], NULL, worker, this);    
        }
        return;
    } while (0);

    // 如果创建不成功释放资源
    if (this->taskQ)    delete this->taskQ;
    if (threadIDs)    delete[] threadIDs;
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    shutDown = true;

    pthread_join(manangerID, NULL);
    for (int i = 0; i < liveNum; i++) {
        pthread_cond_signal(&condHasData);
    }

    if (this->taskQ)  delete this->taskQ;
    if (threadIDs)  delete[] threadIDs;

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&condHasData);
    
}

template <typename T>
void ThreadPool<T>::addTask(Task<T> task)
{
    if (shutDown == 1)  return;
    // 不需要加线程锁，任务队列中已经上锁了
    taskQ->addTask(task);

    pthread_cond_signal(&condHasData);
}

template <typename T>
int ThreadPool<T>::getLiveNum()
{
    int liveNum;
    pthread_mutex_lock(&mutexPool);
    liveNum =  this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return liveNum;
}

template <typename T>
int ThreadPool<T>::getBusyNum()
{
    int busyNum;
    pthread_mutex_lock(&mutexPool);
    busyNum =  this->busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}
 
template <typename T>
void ThreadPool<T>::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < maxNum; ++i)
    {
        if (threadIDs[i] == tid)
        {
            std::cout << "threadExit() function: thread " 
                << pthread_self() << " exiting..." << std::endl;
            threadIDs[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}

template <typename T>
void* ThreadPool<T>::worker(void* arg)
{
    ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);
    
    // 没有销毁就一直执行
    while (1) {
        pthread_mutex_lock(&pool->mutexPool);
        while (pool->taskQ->getTaskNumber() == 0 && !pool->shutDown) {
            // 任务队列未空阻塞
            pthread_cond_wait(&pool->condHasData, &pool->mutexPool);

            // 判断是否销毁线程
            if (pool->exitNum != 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();       // 该工作线程直接结束了，不用return了
                    // 这两行不应该换一下位置么？
                }
            }
        }

        // 自动销毁
        if (pool->shutDown) {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }

        // 从任务队列中取出任务执行
        Task<T> task;
        task = pool->taskQ->takeTask();
        // 修改工作线程busy数量
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexPool);

        // 执行任务
        std::cout << "thread " << pthread_self() << " start working.." << std::endl;
        task.function(task.arg);
        delete task.arg;                 
        task.arg = nullptr;

        std::cout << "thread " << pthread_self() << " end working.." << std::endl;
        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);
    }

    return nullptr;
}


template <typename T>
void* ThreadPool<T>::manager(void* arg)
{
    ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);

    while (!pool->shutDown) {
        // 每隔3s检查一次
        sleep(3);

        // 取出线程池存活的线程数和当前工作的线程数量
        pthread_mutex_lock(&pool->mutexPool);
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        int queueSize = pool->taskQ->getTaskNumber();
        pthread_mutex_unlock(&pool->mutexPool);

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
    return nullptr;
}