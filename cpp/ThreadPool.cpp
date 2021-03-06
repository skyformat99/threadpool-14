#include <unistd.h>

#include "ThreadPool.hpp"
using namespace std;

namespace utils {

void* ThreadPool::ThreadWorker(void* arg)
{
    auto tp = (ThreadPool*)arg;
    auto q = &(tp->m_queue);

    while (true) {
        pthread_mutex_lock(&q->mutex);

        while (q->tasklist.empty())
            pthread_cond_wait(&q->cond, &q->mutex);

        auto t = q->tasklist.front();
        q->tasklist.pop();

        pthread_mutex_unlock(&q->mutex);

        if (!t) {
            pthread_mutex_lock(&tp->m_thread_lock);
            --tp->m_thread_num;
            pthread_mutex_unlock(&tp->m_thread_lock);
            pthread_cond_signal(&tp->m_thread_cond);

            break;
        }

        t->Run();
    }

    return NULL;
}

void ThreadPool::DoAddTask(const shared_ptr<ThreadTask>& t)
{
    pthread_mutex_lock(&m_queue.mutex);
    m_queue.tasklist.push(t);
    pthread_mutex_unlock(&m_queue.mutex);
    pthread_cond_signal(&m_queue.cond);
}

bool ThreadPool::AddTask(const shared_ptr<ThreadTask>& t)
{
    if (m_thread_num == 0)
        return false;

    if (!t)
        return false;

    DoAddTask(t);
    return true;
}

void ThreadPool::DoAddThread()
{
    pthread_t pid;
    if (pthread_create(&pid, NULL, ThreadWorker, this) == 0) {
        pthread_detach(pid);
        pthread_mutex_lock(&m_thread_lock);
        ++m_thread_num;
        pthread_mutex_unlock(&m_thread_lock);
    }
}

void ThreadPool::AddThread(unsigned int num)
{
    for (unsigned int i = 0; i < num; ++i)
        DoAddThread();
}

void ThreadPool::DoDelThread()
{
    DoAddTask(shared_ptr<ThreadTask>());
}

void ThreadPool::DelThread(unsigned int num)
{
    if (num > m_thread_num)
        num = m_thread_num;

    for (unsigned int i = 0; i < num; ++i)
        DoDelThread();
}

ThreadPool::ThreadPool(unsigned int num)
{
    m_thread_num = 0;

    if (num == 0)
        num = sysconf(_SC_NPROCESSORS_CONF) - 1;

    pthread_mutex_init(&m_thread_lock, NULL);
    pthread_cond_init(&m_thread_cond, NULL);

    for (unsigned int i = 0; i < num; ++i)
        DoAddThread();
}

ThreadPool::~ThreadPool()
{
    unsigned int num = m_thread_num;
    for (unsigned int i = 0; i < num; ++i)
        DoDelThread();

    // waiting for remaining task(s) to complete
    pthread_mutex_lock(&m_thread_lock);
    while (m_thread_num > 0)
        pthread_cond_wait(&m_thread_cond, &m_thread_lock);
    pthread_mutex_unlock(&m_thread_lock);

    pthread_cond_destroy(&m_thread_cond);
    pthread_mutex_destroy(&m_thread_lock);
}

}
