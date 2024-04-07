#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working, num = %d\n", pthread_self(), num);
    sleep(2);
    return;
}

int main()
{
    // printf("wang yi\n");
    ThreadPool* pool = createThreadPool(3, 10, 100);
    for (int i = 0; i < 100; i++) {
        int* num = (int*)malloc(sizeof(int));
        *num = 100 + i;
        threadPoolAdd(pool, taskFunc, num);
    }
    sleep(30);

    destoryThreadPool(pool);
    return 0;
}