#pragma once
#include "compat.h"

#if __cplusplus >= 201103L
// C++11 Threads
#include <condition_variable>
#include <mutex>
#include <thread>

#define THREAD_RETURN_T void
#define THREAD_EXIT     return
#define NUM_THREADS     std::thread::hardware_concurrency()

#elif defined(_WIN32) || defined(WIN32)
// C++98/03, Windows Threads
#error "Whoops this isn't ready yet."

#else
#warning "C++11 threads emulated by pthreads"
// C++98/03, POSIX Threads
#include <stdexcept>
#include <pthread.h>
#include <unistd.h>

#define THREAD_RETURN_T void*
#define THREAD_EXIT     return nullptr
#define NUM_THREADS     get_num_threads()

static inline unsigned int get_num_threads()
{
  static unsigned int num_threads = 0;
  if(num_threads)
    return num_threads;

  long rc = sysconf(_SC_NPROCESSORS_ONLN);
  if(rc < 1)
    rc = sysconf(_SC_NPROCESSORS_CONF);
  if(rc < 1)
    throw std::runtime_error("Failed to get number of processors");

  num_threads = rc;
  return rc;
}

namespace std
{

class condition_variable;

class mutex
{
private:
  pthread_mutex_t mtx;

  friend class std::condition_variable;

public:
  mutex()
  {
    int rc = pthread_mutex_init(&mtx, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to initialize mutex");
  }

  void lock()
  {
    int rc = pthread_mutex_lock(&mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to lock mutex");
  }

  void unlock()
  {
    int rc = pthread_mutex_unlock(&mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to unlock mutex");
  }
};

template<typename T>
class unique_lock
{
private:
  T &ulock;
  bool locked;

  friend class std::condition_variable;

public:
  ~unique_lock()
  {
    if(locked)
      ulock.unlock();
  }

  unique_lock(T &ulock)
  : ulock(ulock),
    locked(true)
  {
    ulock.lock();
  }

  void lock()
  {
    ulock.lock();
    locked = true;
  }

  void unlock()
  {
    ulock.unlock();
    locked = false;
  }
};

class condition_variable
{
private:
  pthread_cond_t cond;

public:
  condition_variable()
  {
    int rc = pthread_cond_init(&cond, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to initialize condition variable");
  }

  void wait(std::unique_lock<std::mutex> &ulock)
  {
    int rc = pthread_cond_wait(&cond, &ulock.ulock.mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to wait on condition variable");
  }

  void notify_one()
  {
    int rc = pthread_cond_signal(&cond);
    if(rc != 0)
      throw std::runtime_error("Failed to signal condition variable");
  }

  void notify_all()
  {
    int rc = pthread_cond_broadcast(&cond);
    if(rc != 0)
      throw std::runtime_error("Failed to broadcast condition variable");
  }
};

class thread
{
private:
  pthread_t tid;
  bool      joined;

public:
  thread(void* (*entry)(void*), void *arg)
  : joined(false)
  {
    int rc = pthread_create(&tid, nullptr, entry, arg);
    if(rc != 0)
      throw std::runtime_error("Failed to create thread");
  }

  void join()
  {
    int rc = pthread_join(tid, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to join");
  }
};

}
#endif
