/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of 3dstex.
 *
 * 3dstex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3dstex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 3dstex.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file thread.h
 *  @brief C++11 thread emulation
 */
#pragma once
#include "compat.h"

#if __cplusplus >= 201103L
// C++11 Threads
#include <condition_variable>
#include <mutex>
#include <thread>

/** @brief thread return type */
#define THREAD_RETURN_T void

/** @brief thread return statement */
#define THREAD_EXIT return

#elif defined(_WIN32) || defined(WIN32)
// C++98/03, Windows Threads
#error "Whoops this isn't ready yet."

#else
#warning "C++11 threads emulated by pthreads"
// C++98/03, POSIX Threads
#include <stdexcept>
#include <pthread.h>
#include <unistd.h>

/** @brief thread return type */
#define THREAD_RETURN_T void*

/** @brief thread return statement */
#define THREAD_EXIT return nullptr

namespace std
{

class condition_variable;

/** @brief Emulated std::mutex */
class mutex
{
private:
  pthread_mutex_t mtx; ///< pthread mutex

  friend class std::condition_variable;

public:
  /** @brief Destructor */
  ~mutex()
  {
    int rc = pthread_mutex_destroy(&mtx);
    if(rc != 0)
      std::terminate();
  }

  /** @brief Constructor */
  mutex()
  {
    int rc = pthread_mutex_init(&mtx, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to initialize mutex");
  }

  /** @brief Lock mutex */
  void lock()
  {
    int rc = pthread_mutex_lock(&mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to lock mutex");
  }

  /** @brief Unlock mutex */
  void unlock()
  {
    int rc = pthread_mutex_unlock(&mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to unlock mutex");
  }
};

/** @brief Emulated std::unique_lock
 *  @tparam T Lock type
 */
template<typename T>
class unique_lock
{
private:
  T &ulock;    ///< Unique lock
  bool locked; ///< Whether locked

  friend class std::condition_variable;

public:
  /** @brief Destructor */
  ~unique_lock()
  {
    if(locked)
      ulock.unlock();
  }

  /** @brief Constructor
   *  @param[in] ulock Unique lock
   */
  unique_lock(T &ulock)
  : ulock(ulock),
    locked(true)
  {
    ulock.lock();
  }

  /** @brief Lock the lock */
  void lock()
  {
    ulock.lock();
    locked = true;
  }

  /** @brief Unlock the lock */
  void unlock()
  {
    ulock.unlock();
    locked = false;
  }
};

/** @brief Emulated std::condition_variable */
class condition_variable
{
private:
  pthread_cond_t cond; ///< pthread condition variable

public:
  /** @brief Destructor */
  ~condition_variable()
  {
    int rc = pthread_cond_destroy(&cond);
    if(rc != 0)
      std::terminate();
  }

  /** @brief Constructor */
  condition_variable()
  {
    int rc = pthread_cond_init(&cond, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to initialize condition variable");
  }

  /** @brief Wait for a condition
   *  @param[in] ulock Unique lock to yield
   */
  void wait(std::unique_lock<std::mutex> &ulock)
  {
    int rc = pthread_cond_wait(&cond, &ulock.ulock.mtx);
    if(rc != 0)
      throw std::runtime_error("Failed to wait on condition variable");
  }

  /** @brief Signal a condition waiter */
  void notify_one()
  {
    int rc = pthread_cond_signal(&cond);
    if(rc != 0)
      throw std::runtime_error("Failed to signal condition variable");
  }

  /** @brief Signal all condition waiters */
  void notify_all()
  {
    int rc = pthread_cond_broadcast(&cond);
    if(rc != 0)
      throw std::runtime_error("Failed to broadcast condition variable");
  }
};

/** @brief Emulated std::thread
 *  @note This does not call std::terminate() if an unjoined thread is
 *  destroyed since we do not have move semantics available. It can only
 *  wrap pthread entry functions since we don't have perfect forwarding or
 *  variadic templates.
 */
class thread
{
private:
  pthread_t tid; ///< pthread id

public:
  /** @brief Constructor
   *  @param[in] entry Entry function
   *  @param[in] arg   Argument to pass to entry function
   */
  thread(void* (*entry)(void*), void *arg)
  {
    int rc = pthread_create(&tid, nullptr, entry, arg);
    if(rc != 0)
      throw std::runtime_error("Failed to create thread");
  }

  /** @brief Join thread */
  void join()
  {
    int rc = pthread_join(tid, nullptr);
    if(rc != 0)
      throw std::runtime_error("Failed to join");
  }

  /** @brief Get hardware concurrency
   *  @returns hardware concurrency
   */
  static unsigned int hardware_concurrency()
  {
    static unsigned int num_cores = 0;
    if(num_cores)
      return num_cores;

    /* try to get number of online cores */
    long rc = sysconf(_SC_NPROCESSORS_ONLN);

    /* fallback to number of configured cores */
    if(rc < 1)
      rc = sysconf(_SC_NPROCESSORS_CONF);

    /* error */
    if(rc < 1)
      throw std::runtime_error("Failed to get number of processors");

    /* memoize number of cores */
    num_cores = rc;
    return rc;
  }
};

}
#endif
