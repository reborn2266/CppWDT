#ifndef _MC_WATCHDOG_HXX_
#define _MC_WATCHDOG_HXX_

#include <pthread.h>

namespace MC {

class WatchdogExp: public std::exception {};

class Watchdog
{
   protected:
      void resetMembers(unsigned int timeout_period);
      int setNonblocking(int fd);
      int startOnce();
      static void* timeoutChecker(void *threadID);
      static void* kickedChecker(void *threadID);

   public:
      Watchdog(unsigned int timeout_period=10);
      void start();
      void kick();

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

}//end of ns Trend

#endif //end of _MC_WATCHDOG_HXX_
