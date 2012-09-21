#include <iostream>
#include <exception>
#include <ctime>
#include <cstdlib>
#include <memory>

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#include "wdt.hpp"

namespace MC {

int Watchdog::resetMembers(unsigned int timeout_period)
{
   m_timeout_period = timeout_period;
   m_is_timeout = false;
   m_last_kicked_time = std::time(NULL);
   m_worker_pid = -1;
   m_is_stop = false;

   int pfd[2];

   if (::pipe(pfd) == -1)
   {
      return -1;
   }

   m_pipe_r = pfd[0];
   m_pipe_w = pfd[1];

   ::pthread_mutex_init(&m_lock, NULL);
   return 0;
}
#if 1
int Watchdog::setNonblocking(int fd)
{
   int flags;

   if (-1 == (flags = ::fcntl(fd, F_GETFL, 0)))
   {
      flags = 0;
   }
   return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int Watchdog::startOnce()
{
   switch (m_worker_pid = ::fork())
   {
      case -1:
         throw WatchdogExp();

      case 0:
         //child, go back to do its work
         ::close(m_pipe_r);
         return 1;

      default:
         int ret = -1;

         ::close(m_pipe_w);

         ret = setNonblocking(m_pipe_r);
         if (ret)
         {
            throw WatchdogExp();
         }

         ret = ::pthread_create(&m_timeoutChecker_handle, NULL, timeoutChecker, this);
         if (ret)
         {
            throw WatchdogExp();
         }

         ret = ::pthread_create(&m_kickedChecker_handle, NULL, kickedChecker, this);
         if (ret)
         {
            ::pthread_mutex_lock(&m_lock);
            m_is_stop = true;
            ::pthread_mutex_unlock(&m_lock);
            ::pthread_join(m_timeoutChecker_handle, NULL);
            throw WatchdogExp();
         }

         while (true)
         {
            ::pthread_mutex_lock(&m_lock);

            if (m_is_timeout)
            {
               m_is_stop = true;
               ::kill(m_worker_pid, SIGKILL);
               ::waitpid(m_worker_pid, NULL, 0);
               ::close(m_pipe_r);
               ::pthread_mutex_unlock(&m_lock);
               break;
            }

            ::pthread_mutex_unlock(&m_lock);
            ::sleep(1);
         }

         ::pthread_join(m_timeoutChecker_handle, NULL);
         ::pthread_join(m_kickedChecker_handle, NULL);
         ::pthread_mutex_destroy(&m_lock);
         break;
   }

   return 0;
}

void* Watchdog::timeoutChecker(void *threadID)
{
   Watchdog *wdt = static_cast<Watchdog *> (threadID);

   while (true)
   {
      time_t cur_time = time(NULL);

      ::pthread_mutex_lock(&wdt->m_lock);

      if (wdt->m_is_stop)
      {
         ::pthread_mutex_unlock(&wdt->m_lock);
         break;
      }

      std::cout << "check timeout" << std::endl;

      if ((cur_time - wdt->m_last_kicked_time) > wdt->m_timeout_period)
      {
         std::cout << "timeout" << std::endl;
         wdt->m_is_timeout = true;
      }
      else
      {
         wdt->m_is_timeout = false;
      }

      ::pthread_mutex_unlock(&wdt->m_lock);

      ::sleep(1);
   }

   return NULL;
}

void* Watchdog::kickedChecker(void *threadID)
{
   Watchdog *wdt = static_cast<Watchdog *> (threadID);
   fd_set readfds;
   char err_str[64];

   while (true)
   {
      struct timeval tv;

      tv.tv_sec = 1;
      tv.tv_usec = 0;
      ::pthread_mutex_lock(&wdt->m_lock);
      if (wdt->m_is_stop)
      {
         ::pthread_mutex_unlock(&wdt->m_lock);
         break;
      }
      ::pthread_mutex_unlock(&wdt->m_lock);

      FD_ZERO(&readfds);
      FD_SET(wdt->m_pipe_r, &readfds);

      int ret = select(wdt->m_pipe_r+1, &readfds, NULL, NULL, &tv);
      if (ret == -1)
      {
      }
      else if (ret)
      {
         char data = '\0';
         int n_read = 0;

         if (FD_ISSET(wdt->m_pipe_r, &readfds))
         {
            n_read = ::read(wdt->m_pipe_r, &data, 1);
            if (data == 'Y')
            {
               std::cout << "sense kicked" << std::endl;
               ::pthread_mutex_lock(&wdt->m_lock);
               wdt->m_last_kicked_time = std::time(NULL);
               ::pthread_mutex_unlock(&wdt->m_lock);
            }
         }
      }
   }
   return NULL;
}

Watchdog::Watchdog(unsigned int timeout_period)
{
   int ret = -1;

   if (resetMembers(timeout_period) < 0)
   {
      throw WatchdogExp();
   }
}

void Watchdog::start()
{
   while (true)
   {
      // if it is child, startOnce will return non-zero
      // else, return zero to indicate wdt want to restart worker
      if (startOnce())
      {
         break;
      }
      else
      {
         std::cout << "restart" << std::endl;
         // reset all members to restart again
         resetMembers(m_timeout_period);
      }
   }
}

int Watchdog::kick()
{
   int ret = -1;
   const char data = 'Y';

   ::pthread_mutex_lock(&m_lock);
   ret = ::write(m_pipe_w, &data, 1);
   if (ret != 1)
   {
      ::pthread_mutex_unlock(&m_lock);
      return -1;
   }
   std::cout << "kick" << std::endl;
   ::pthread_mutex_unlock(&m_lock);
   return 1;
}

}//end of ns MC
#endif
