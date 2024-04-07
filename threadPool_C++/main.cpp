#include <iostream>
#include "threadPool.h"
#include "threadPool.cpp"

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    std::cout << "thread " << pthread_self() << " is working, num = " << num << std::endl;
    sleep(2);
    return;
}

int main()
{
    // printf("wang yi\n");
    ThreadPool<int> pool(3, 10);
    for (int i = 0; i < 100; i++) {
        int* num = new int(100 + i);
        pool.addTask(Task<int>(taskFunc, num));
    }
    sleep(30);

    return 0;
}