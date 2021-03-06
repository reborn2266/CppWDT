#include <memory>
#include <iostream>
#include <unistd.h>

#include "wdt.hpp"

class Worker
{
   private:
      std::shared_ptr<MC::Watchdog> m_wdt;

   public:
      Worker(std::shared_ptr<MC::Watchdog> wdt): m_wdt(wdt) {}

      void run()
      {
         char *p = NULL;
         while (true)
         {
            std::cout << "[Worker] running" << std::endl;
            char a = *(p+1);
            std::cout << a << std::endl;
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
   w.run();
}
