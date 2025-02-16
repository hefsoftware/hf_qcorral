#pragma once
#include <limits>
#include "../corral.h"
#include <QDebug>
class QCoreApplication;
#include <QTimer>
#include <QElapsedTimer>

namespace detail {
class Timer;
class TimerInstance {
  friend class Timer;
public:
  TimerInstance(int64_t delayNs): m_delayNs(delayNs) {
    if(delayNs==0)
      m_fired=true;
    else if(delayNs>0)
      m_elapsed.start();
  }
  ~TimerInstance() {
    delete m_timer;
  }
protected:
  struct Awaiter;
  bool start(Awaiter *awaiter, std::weak_ptr<TimerInstance> self);
  void stop(Awaiter *awaiter);
  void fired();
  struct Awaiter {
    Awaiter(std::shared_ptr<TimerInstance> timer): m_timer(timer) {}
    std::shared_ptr<TimerInstance> m_timer;
    bool await_ready() const noexcept {
      return m_timer->m_fired;
    }
    bool await_early_cancel() noexcept {
      m_earlyCancel=true;
      return false;
    }
    bool await_suspend(corral::Handle h) {
      bool ret=false;
      if(!m_earlyCancel) {
        ret=m_timer->start(this, m_timer);
        if(ret)
          m_suspended=h;
      }
      return ret;
    }
    bool await_cancel(corral::Handle handle) noexcept {
      m_timer->stop(this);
      // We do handle.resume() and return false so await_must_resume will be called and notify we actually got a result
      handle.resume();
      return false;
    }
    bool await_must_resume() const noexcept {
      // We always have a valid result
      return true;
    }
    bool await_resume() {
      m_timer->stop(this);
      return m_timer->m_fired;
    }
    void await_introspect(corral::detail::TaskTreeCollector &tree) const noexcept {
      tree.node("QtTimer");
    }
    void onFired() {
      if(m_suspended)
        std::exchange(m_suspended, nullptr).resume();
    }
  private:
    bool m_earlyCancel=false;
    corral::Handle m_suspended={};
  };
  int64_t m_delayNs;
  bool m_fired=false;
  QElapsedTimer m_elapsed;
  QTimer *m_timer=nullptr;
  QSet<Awaiter *> m_awaiting;
};

class Timer {
public:
  inline Timer(int64_t nanoSeconds): m_timer{std::make_shared<TimerInstance>(nanoSeconds)} {
  }
  template <class R, class P>
  inline Timer(std::chrono::duration<R, P> delay): m_timer{std::make_shared<TimerInstance>(toNanoSeconds(delay))} {
  }
  TimerInstance::Awaiter operator co_await() { return TimerInstance::Awaiter(m_timer); }
private:
  static constexpr int64_t toNanoSeconds(auto duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
            .count();
  }
  int64_t m_timeoutNs;
  std::shared_ptr<TimerInstance> m_timer;
};

}
/// A utility function, returning an awaitable suspending the caller
/// for specified duration. Suitable for use with anyOf() etc.
template <class R, class P>
static inline auto qSleepFor(std::chrono::duration<R, P> delay) {
    return detail::Timer(delay);
}
static inline auto qSleepFor(int64_t ms) {
  return detail::Timer(ms*1000000);
}
static inline auto qSleepForNs(int64_t ns) {
    return detail::Timer(ns);
}

template <class Awaitable, class R, class P> static inline corral::Task<std::optional<corral::detail::AwaitableReturnType<Awaitable>>> qAwaitTimeout(std::chrono::duration<R,P> delay, Awaitable &&awaitable) {
  auto result=co_await corral::anyOf(qSleepFor(delay), std::move(awaitable));
  co_await corral::yield;
  co_return std::get<1>(result);
}
namespace details {
template <typename T>
std::reference_wrapper<T> convert (T & t)
{ return t; }

template <typename T>
T convert (T && t)
{ return std::move(t); }

template <typename ... Ts>
auto make_comparible (Ts && ... args)
{ return std::make_tuple(convert(std::forward<Ts>(args))...); }
}

struct CorralQt {
  friend struct corral::EventLoopTraits<QCoreApplication>;
public:
  static void exec(QCoreApplication &app);
  template <class Callable, class... Args>
  static void run(Callable c, Args &&...args) {
    if(g_defaultNursery) {
      qDebug()<<"About to start";
      g_defaultNursery->start(c, std::forward<decltype(args)>(args)...);
    }
    else {
      // if(!g_nursery)
      //   g_nursery=new corral::UnsafeNursery;
      // g_nursery->start(start(c, std::forward<decltype(args)>(args)...););
      if(!g_waitingStart)
        g_waitingStart=new std::vector<std::function<corral::Task<void>()>>;
      g_waitingStart->emplace_back([f=std::move(c), args=details::make_comparible(std::forward<Args>(args)...)](){ return std::apply(f, std::move(args)); });
    }
  }
  // inline auto sleepFor(boost::asio::io_service& io,
  //                      boost::posix_time::time_duration delay) {
  //     return detail::Timer(io, delay);
  // }
  static corral::Nursery &defaultNursery() { return *g_defaultNursery; }
private:
  static corral::Task<void> mainNursery();
  static corral::Nursery *g_defaultNursery;
  // We can't directly save the generated task as this would
  static std::vector<std::function<corral::Task<void>()>> *g_waitingStart;
  // static corral::UnsafeNursery *g_nursery;
};
