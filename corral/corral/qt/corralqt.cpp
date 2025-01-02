#include "corralqt.h"
#include "../corral.h"
#include <QDebug>
#include <QCoreApplication>

corral::Nursery *CorralQt::g_defaultNursery=nullptr;
std::vector<std::function<corral::Task<void>()>> *CorralQt::g_waitingStart=nullptr;
// Corral::UnsafeNursery *CorralQt::g_nursery=nullptr;

namespace corral {
template <> struct EventLoopTraits<QCoreApplication> {
    static EventLoopID eventLoopID(QCoreApplication& app) {
        return EventLoopID(&app);
    }
    static void run(QCoreApplication& app) {
        QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
          // qDebug()<<"About to quit";
          CorralQt::g_defaultNursery->cancel();
        });
        app.exec();
    }
    static void stop(QCoreApplication& app) {
        // qDebug()<<"Stop core";
        app.exit();
    }
};
}

void CorralQt::exec(QCoreApplication &app) {
  corral::run(app, mainNursery());
}


corral::Task<void> CorralQt::mainNursery()
{
  CORRAL_WITH_NURSERY(nursery)
  {
    g_defaultNursery = &nursery;
    if(g_waitingStart) {
      for(auto &f: *g_waitingStart)
        nursery.start(f);
      delete g_waitingStart;
      g_waitingStart=nullptr;
    }
    co_await corral::SuspendForever{};
  };
}

bool detail::TimerInstance::start(Awaiter *awaiter, std::weak_ptr<TimerInstance> self)
{
  if (m_fired) {
    // Timeout already fired
    return false;
  }
  else if(m_delayNs<0) {
    // Infinite timeout
    return true;
  }
  else {
    int64_t sleep=m_delayNs-m_elapsed.nsecsElapsed();
    if(sleep<=0) {
      fired();
      return false;
    }
    else {
      m_awaiting.insert(awaiter);
      if(!m_timer) {
        m_timer=new QTimer;
        m_timer->setSingleShot(true);
        m_timer->connect(m_timer, &QTimer::timeout, [this, self]() {
          // Makes sure timer instance is not destroyed while serving the fired event
          if(auto ptr=self.lock())
            ptr->fired();
        });
        m_timer->start(sleep/1000000);
      }
      return true;
    }
  }
}

void detail::TimerInstance::stop(Awaiter *awaiter) {
  m_awaiting.remove(awaiter);
  if(m_awaiting.isEmpty()) {
    delete m_timer;
    m_timer=nullptr;
  }
}

void detail::TimerInstance::fired() {
  m_fired=true;
  delete m_timer;
  m_timer=nullptr;
  while(!m_awaiting.isEmpty()) {
    auto it=m_awaiting.begin();
    auto awaiting=*it;
    m_awaiting.erase(it);
    awaiting->onFired();
  }
}
