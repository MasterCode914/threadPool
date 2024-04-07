#include "taskQueue.h"

template <typename T>
TaskQueue<T>::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T> t)
{
    pthread_mutex_lock(&m_mutex);
    m_queue.push(t);
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(callback f, void* arg)
{
    pthread_mutex_lock(&m_mutex);
    m_queue.push(Task<T>(f, arg));
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
Task<T> TaskQueue<T>::takeTask()
{
    Task<T> t;
    pthread_mutex_lock(&m_mutex);
    if (getTaskNumber() > 0) {      // 这里就不阻塞任务队列了
        t = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return t;
}
