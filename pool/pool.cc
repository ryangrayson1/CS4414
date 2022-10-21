#include "pool.h"

Task::Task() {
}

Task::~Task() {
}

ThreadPool::ThreadPool(int num_threads) {
}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
}

void ThreadPool::WaitForTask(const std::string &name) {
}

void ThreadPool::Stop() {
}
