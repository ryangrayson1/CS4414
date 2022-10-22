#include "pool.h"
using namespace std;

Task::Task() {
}

Task::~Task() {
}

void ThreadPool::RunThread() {
    while (true) {
        pthread_mutex_lock(&queue_lock);
        while (task_queue.empty()) {
            pthread_cond_wait(&task_ready, &queue_lock);
            pthread_mutex_lock(&stop_lock);
            if (stop) {
                pthread_mutex_unlock(&stop_lock);
                pthread_mutex_unlock(&queue_lock);
                cout << "Thread " << getpid() << " Finished\n";
                return;
            }
            pthread_mutex_unlock(&stop_lock);
        }
        Task *task = task_queue.front();
        task_queue.pop();
        pthread_mutex_unlock(&queue_lock);
        task->Run();
        pthread_mutex_lock(&(task->done_lock));
        task->done = true;
        pthread_cond_broadcast(&(task->done_cv));
        pthread_mutex_unlock(&(task->done_lock));
    }
}

void *run_thread_helper(void *instance) {
    ThreadPool* tptr = (ThreadPool*)instance;
    tptr->RunThread();
    return NULL;
}

ThreadPool::ThreadPool(int num_threads) {
    pthread_mutex_init(&stop_lock, NULL);
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&map_lock, NULL);
    pthread_cond_init(&task_ready, NULL);
    for (int i = 0; i < num_threads; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, run_thread_helper, (void*)this);
        pthread_join(thread, NULL);
    }
}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    pthread_mutex_init(&(task->done_lock), NULL);
    pthread_cond_init(&(task->done_cv), NULL);
    
    pthread_mutex_lock(&(task->done_lock));
    task->done = false;
    pthread_mutex_unlock(&(task->done_lock));

    pthread_mutex_lock(&map_lock);
    task_map[name] = task;
    pthread_mutex_unlock(&map_lock);

    pthread_mutex_lock(&queue_lock);
    task_queue.push(task);
    pthread_cond_signal(&task_ready);
    pthread_mutex_unlock(&queue_lock);
}

void ThreadPool::WaitForTask(const std::string &name) {
    pthread_mutex_lock(&map_lock);
    Task *task = task_map[name];
    pthread_mutex_unlock(&map_lock);

    pthread_mutex_lock(&task->done_lock);
    while (!task->done) {
        pthread_cond_wait(&(task->done_cv), &(task->done_lock));
    }
    delete task;
    pthread_mutex_unlock(&task->done_lock);
    
}

void ThreadPool::Stop() {
    pthread_mutex_lock(&stop_lock);
    stop = true;
    pthread_cond_broadcast(&task_ready);
    pthread_mutex_unlock(&stop_lock);
}
