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

namespace MC {

class WatchdogExp: public std::exception {
   public:
      WatchdogExp();
};

WatchdogExp::WatchdogExp() {}

class Watchdog
{
   protected:
      void resetMembers(unsigned int timeout_period)
      {
         m_timeout_period = timeout_period;
         m_is_timeout = false;
         m_last_kicked_time = std::time(NULL);
         m_worker_pid = -1;
         m_is_stop = false;

         int pfd[2];

         if (::pipe(pfd) == -1)
         {
            std::cerr << "create PIPE failed" << std::endl;
            throw WatchdogExp();
         }

         m_pipe_r = pfd[0];
         m_pipe_w = pfd[1];

         ::pthread_mutex_init(&m_lock, NULL);
      }

      int setNonblocking(int fd)
      {
         int flags;
         if (-1 == (flags = ::fcntl(fd, F_GETFL, 0)))
         {
            flags = 0;
         }
         return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      }

      int startOnce()
      {
         switch (m_worker_pid = ::fork())
         {
            case -1:
               std::cerr << "fork failed" << std::endl;
               std::exit(-1);

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
                  std::cerr << "set nonblocking failed" << std::endl;
                  throw WatchdogExp();
               }

               ret = ::pthread_create(&m_timeoutChecker_handle, NULL, timeoutChecker, this);
               if (ret)
               {
                  std::cerr << "create timeoutChecker failed" << std::endl;
                  throw WatchdogExp();
               }

               ret = ::pthread_create(&m_kickedChecker_handle, NULL, kickedChecker, this);
               if (ret)
               {
                  std::cerr << "create timeoutChecker failed" << std::endl;
                  throw WatchdogExp();
               }

               while (true)
               {
                  std::cout << "[WDT] check timeout" << std::endl;
                  ::pthread_mutex_lock(&m_lock);

                  if (m_is_timeout)
                  {
                     std::cerr << "[WDT] timeout!!" << std::endl;
                     m_is_stop = true;
                     std::cout << "[WDT] kill worker" << std::endl;
                     ::kill(m_worker_pid, SIGKILL);
                     ::waitpid(m_worker_pid, NULL, 0);
                     std::cout << "[WDT] recycle worker done" << std::endl;
                     ::close(m_pipe_r);
                     ::pthread_mutex_unlock(&m_lock);
                     break;
                  }

                  ::pthread_mutex_unlock(&m_lock);
                  ::sleep(1);
               }

               std::cout << "[WDT] wait monitor threads" << std::endl;
               ::pthread_join(m_timeoutChecker_handle, NULL);
               ::pthread_join(m_kickedChecker_handle, NULL);
               std::cout << "[WDT] recycle monitor threads done" << std::endl;
               ::pthread_mutex_destroy(&m_lock);
               break;
         }

         return 0;
      }

      static void* timeoutChecker(void *threadID)
      {
         Watchdog *wdt = reinterpret_cast<Watchdog *> (threadID);

         while (true)
         {
            time_t cur_time = time(NULL);

            ::pthread_mutex_lock(&wdt->m_lock);

            if (wdt->m_is_stop)
            {
               ::pthread_mutex_unlock(&wdt->m_lock);
               break;
            }

            if ((cur_time - wdt->m_last_kicked_time) > wdt->m_timeout_period)
            {
               wdt->m_is_timeout = true;
            }
            else
            {
               wdt->m_is_timeout = false;
            }

            ::pthread_mutex_unlock(&wdt->m_lock);

            ::sleep(1);
         }
      }

      static void* kickedChecker(void *threadID)
      {
         Watchdog *wdt = reinterpret_cast<Watchdog *> (threadID);

         while (true)
         {
            char data = '\0';
            int n_read = 0;

            ::pthread_mutex_lock(&wdt->m_lock);

            if (wdt->m_is_stop)
            {
               ::pthread_mutex_unlock(&wdt->m_lock);
               break;
            }

            // this is a nonblocking read. Why?
            // think about this: if no one  write to pipe, we might be blocked in read. then,
            // how can we detect if we are forced to stop kickedChecker?
            n_read = ::read(wdt->m_pipe_r, &data, 1);
            if ((n_read == -1) && (errno != EAGAIN))
            {
               std::cerr << "read pipe("<< wdt->m_pipe_r << ") wrong, errno("<< errno <<")" << std::endl;
               ::pthread_mutex_unlock(&wdt->m_lock);
               throw WatchdogExp();
            }

            if (data == 'Y')
            {
               std::cout << "[WDT] sense kicked" << std::endl;
               wdt->m_last_kicked_time = std::time(NULL);
            }

            ::pthread_mutex_unlock(&wdt->m_lock);
            ::sleep(1);
         }
      }

   public:
      Watchdog(unsigned int timeout_period=10)
      {
         resetMembers(timeout_period);
      }

      void start()
      {
         int ret = -1;

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
               std::cout << "[WDT] restart worker" << std::endl;
               // reset all members to restart again
               resetMembers(m_timeout_period);
            }
         }
      }

      void kick()
      {
         int ret = -1;
         char data = 'Y';

         ::pthread_mutex_lock(&m_lock);

         ret = ::write(m_pipe_w, &data, 1);
         if (ret != 1)
         {
            std::cerr << "kick WDT failed" << std::endl;
            ::pthread_mutex_unlock(&m_lock);
            throw WatchdogExp();
         }

         ::pthread_mutex_unlock(&m_lock);
         std::cout << "[WDT] kick happen" << std::endl;
      }

   private:
      unsigned int m_timeout_period;
      bool m_is_timeout;
      time_t m_last_kicked_time;
      int m_pipe_r;
      int m_pipe_w;
      pthread_t m_timeoutChecker_handle;
      pthread_t m_kickedChecker_handle;
      pid_t m_worker_pid;
      pthread_mutex_t m_lock;
      bool m_is_stop;
};

}//end of ns MC

class Worker
{
   private:
      std::shared_ptr<MC::Watchdog> m_wdt;

   public:
      Worker(std::shared_ptr<MC::Watchdog> wdt): m_wdt(wdt) {}

      void run()
      {
         while (true)
         {
            std::cout << "[Worker] running" << std::endl;
            ::sleep(2);
            m_wdt->kick();
         }
      }
};

int main(int argc, char **argv)
{
   std::shared_ptr<MC::Watchdog> wdt(new MC::Watchdog(5));
   Worker w(wdt);
   int ret = -1;

   wdt->start();
   while (true)
   {
      w.run();
   }
}
