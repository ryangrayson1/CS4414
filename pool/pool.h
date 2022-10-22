#ifndef POOL_H_
#include <iostream>
#include <unistd.h>
#include <string>
#include <pthread.h>
#include <queue>
#include <unordered_map>
using namespace std;

class Task {
public:

    bool done;
    pthread_cond_t done_cv;
    pthread_mutex_t done_lock;

    Task();

    virtual ~Task();

    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:

    bool stop = false;
    pthread_mutex_t stop_lock;

    queue<Task*> task_queue;
    pthread_mutex_t queue_lock;

    unordered_map<string, Task*> task_map;
    pthread_mutex_t map_lock;

    pthread_cond_t task_ready;

    ThreadPool(int num_threads);

    void RunThread();

    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();
};
#endif
