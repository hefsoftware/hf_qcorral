#pragma once
#include <QIODevice>
#include <QString>
#include <functional>
#include "corralqt.h"
class CorralQIODevice;
class CorralQIODeviceInstance //, public std::enable_shared_from_this<CorralQIODeviceInstance>
{
  friend class CorralQIODevice;
  class AwaiterBase;
  CorralQIODeviceInstance(QIODevice *device): m_device(device) {}

  std::unique_ptr<QIODevice> m_device;
  QByteArray m_buffer;
  AwaiterBase *m_awaiter=nullptr;
  QMetaObject::Connection m_connection;
  size_t tryRead(size_t n) {
    QByteArray temp=m_device->read(n);
    m_buffer+=temp;
    return temp.size();
  }
  void discardReadBuffer() {
    while(tryRead(1000)) { }
    qDebug()<<m_buffer;
    m_buffer.clear();
  }

  class AwaiterBase {
      friend class CorralQIODeviceInstance;
  public:
      AwaiterBase(std::shared_ptr<CorralQIODeviceInstance> source): m_source(source) { }
  protected:
      size_t tryRead(size_t n) const { return m_source->tryRead(n); }
      QByteArray &buffer() const { return m_source->m_buffer; }
      void startListen() {
        m_source->startListen(*this);
      }
      void endListen() {
        m_source->endListen(*this);
      }
      virtual void newData()=0;
  private:
      std::shared_ptr<CorralQIODeviceInstance> m_source;
  };
  void startListen(AwaiterBase &awaiter) {
    Q_ASSERT(!m_awaiter);
    m_awaiter=&awaiter;
    m_connection=QObject::connect(m_device.get(), &QIODevice::readyRead, [this](){ onNewData(); });
  }
  void endListen(AwaiterBase &awaiter) {
    Q_ASSERT(m_awaiter==&awaiter);
    m_awaiter=nullptr;
    QObject::disconnect(m_connection);
  }

  void onNewData() {
    Q_ASSERT(m_awaiter);
    m_awaiter->newData();
  }
  template <typename T> class NonCancellableAwaiter;
  template <typename T> class CancellableAwaiter;
public:
  template <typename T> class NonCancellableAwaited {
  public:
    NonCancellableAwaited(std::shared_ptr<CorralQIODeviceInstance> source)
      : m_source(source)
    {}
    NonCancellableAwaiter<T> operator co_await();
    /**
     * @brief Checks if we already have data to return before even awaiting.
     *
     * If the class has some cache on the buffer the implementation should clear it in this function.
     * @param buffer Data buffer
     * @return True if we can skip the await as we already have a result
     */
    virtual bool initialCheck(QByteArray &buffer) noexcept=0;
    virtual bool check(QByteArray &buffer) noexcept=0;
    virtual T result(QByteArray &buffer)=0;
    /**
       * @brief The name of the awaiter
       *
       * This is the name that should be shown when inspecting the data.
       * @return Name of the awaiter
       */
    virtual const char *name() { return "CorralQIODevice::NonCancellable"; }
  private:
    std::shared_ptr<CorralQIODeviceInstance> m_source;
  };
  template <typename T> class CancellableAwaited {
  public:
      CancellableAwaited(std::shared_ptr<CorralQIODeviceInstance> source)
          : m_source(source)
      {}
      CancellableAwaiter<T> operator co_await();
      /**
     * @brief Checks if we already have data to return before even awaiting.
     *
     * If the class has some cache on the buffer the implementation should clear it in this function.
     * @param buffer Data buffer
     * @return True if we can skip the await as we already have a result
     */
      virtual bool initialCheck(QByteArray &buffer) noexcept=0;
      virtual bool check(QByteArray &buffer) noexcept=0;
      virtual T result(QByteArray &buffer)=0;
      /**
       * @brief The name of the awaiter
       *
       * This is the name that should be shown when inspecting the data.
       * @return Name of the awaiter
       */
      virtual const char *name() { return "CorralQIODevice::Cancellable"; }
  private:
      std::shared_ptr<CorralQIODeviceInstance> m_source;
  };
  private:
  template <typename T> struct NonCancellableAwaiter: AwaiterBase {
    NonCancellableAwaiter(std::shared_ptr<CorralQIODeviceInstance> source, NonCancellableAwaited<T> &handler): AwaiterBase(source), m_handler(handler) {}
    bool check() const noexcept {
      bool ret=false;
      while(!ret && tryRead(128))
        ret=m_handler.check(buffer());
      return ret;
    }
    bool await_ready() const noexcept {
      bool ret=m_handler.initialCheck(buffer());
      if(!ret)
        ret=check();
      return ret;
    }
    bool await_early_cancel() noexcept { m_earlyCancel=true; return false; }
    bool await_suspend(corral::Handle h) {
      bool ready=check();
      if(!ready && !m_earlyCancel) {
        m_suspended=h;
        m_listening=true;
        startListen();
      }
      return !ready;
    }
    bool await_must_resume() const noexcept {
      // We will always have a result
      return true;
    }
    bool await_cancel(corral::Handle handle) noexcept {
      // m_timer->stop(this);
      // // We do handle.resume() and return false so await_must_resume will be called and notify we actually got a result
      handle.resume();
      return false;
    }
    T await_resume() {
      if(m_listening) {
        m_listening=false;
        endListen();
      }
      return m_handler.result(buffer());
    }
    void await_introspect(corral::detail::TaskTreeCollector &tree) const noexcept {
      tree.node(m_handler.name());
    }
    void newData() override {
      bool finished=check();
      if(finished&&m_suspended)
        std::exchange(m_suspended, nullptr).resume();
    }
  private:
    NonCancellableAwaited<T> &m_handler;
    corral::Handle m_suspended={};
    bool m_earlyCancel=false;
    bool m_listening=false;
  };
  template <typename T> struct CancellableAwaiter: AwaiterBase {
      CancellableAwaiter(std::shared_ptr<CorralQIODeviceInstance> source, CancellableAwaited<T> &handler): AwaiterBase(source), m_handler(handler) {}
      bool check() const noexcept {
          bool ret=false;
          while(!ret && tryRead(128))
              ret=m_handler.check(buffer());
          return ret;
      }
      bool await_ready() const noexcept {
          bool ret=m_handler.initialCheck(buffer());
          if(!ret)
              ret=check();
          return ret;
      }
      std::true_type await_early_cancel() noexcept { return {}; }
      bool await_suspend(corral::Handle h) {
          bool ready=check();
          if(!ready) {
              m_suspended=h;
              m_listening=true;
              startListen();
          }
          return !ready;
      }
      std::true_type await_cancel(corral::Handle handle) noexcept {
          if(m_listening) {
              m_listening=false;
              endListen();
          }
          return {};
      }
      T await_resume() {
          if(m_listening) {
              m_listening=false;
              endListen();
          }
          return m_handler.result(buffer());
      }
      void await_introspect(corral::detail::TaskTreeCollector &tree) const noexcept {
        tree.node(m_handler.name());
      }
      void newData() override {
          bool finished=check();
          if(finished&&m_suspended)
              std::exchange(m_suspended, nullptr).resume();
      }
  private:
      CancellableAwaited<T> &m_handler;
      corral::Handle m_suspended={};
      bool m_listening=false;
  };
};
template <typename T> CorralQIODeviceInstance::NonCancellableAwaiter<T> CorralQIODeviceInstance::NonCancellableAwaited<T>::operator co_await() { return NonCancellableAwaiter<T>{m_source, *this}; }
template <typename T> CorralQIODeviceInstance::CancellableAwaiter<T> CorralQIODeviceInstance::CancellableAwaited<T>::operator co_await() { return CancellableAwaiter<T>{m_source, *this}; }

class CorralQIODevice {
  template <typename T> class Awaiter;
public:
  CorralQIODevice() {}
  operator bool() { return (bool) m_instance; }
  void discardReadBuffer() {
    m_instance->discardReadBuffer();
  }
  static CorralQIODevice create(QIODevice *device);
  static CorralQIODevice open(QIODevice *device, QIODeviceBase::OpenMode mode=QIODevice::ReadWrite);
  bool writeChar(char ch);
  bool write(const QByteArray &data);
  template <typename T> class NonCancellableAwaited: public CorralQIODeviceInstance::NonCancellableAwaited<T> {
  public:
    NonCancellableAwaited(CorralQIODevice source): CorralQIODeviceInstance::NonCancellableAwaited<T>(source.m_instance) {}
  };
  template <typename T> class CancellableAwaited: public CorralQIODeviceInstance::CancellableAwaited<T> {
  public:
    CancellableAwaited(CorralQIODevice source): CorralQIODeviceInstance::CancellableAwaited<T>(source.m_instance) {}
  };
private:
  QIODevice *device();
  CorralQIODevice(QIODevice *device);
  std::shared_ptr<CorralQIODeviceInstance> m_instance={};
  class DiscardAwaited: public NonCancellableAwaited<void> {
  public:
      DiscardAwaited(CorralQIODevice device): NonCancellableAwaited<void>{device} {}
      bool initialCheck(QByteArray &buffer) noexcept override { return false; }
      bool check(QByteArray &buffer) noexcept override { return false; }
      void result(QByteArray &buffer) override { buffer.clear(); }
      const char *name() override { return "CorralQIODevice::Discard"; }
  };
  class ReadAwaited: public NonCancellableAwaited<QByteArray> {
  public:
    ReadAwaited(CorralQIODevice device, size_t n, bool partial): NonCancellableAwaited<QByteArray>{device}, m_n(n), m_partial{partial} {}
    bool initialCheck(QByteArray &buffer) noexcept override;
    bool check(QByteArray &buffer) noexcept override;
    QByteArray result(QByteArray &buffer) override;
    const char *name() override { return "CorralQIODevice::ReadAwaited"; }
  protected:
      size_t m_n;
      bool m_partial;
  };
  template <typename T> class ReadIntAwaited final: public CancellableAwaited<T> {
  public:
      ReadIntAwaited(CorralQIODevice device): CancellableAwaited<T>{device} {}
      bool initialCheck(QByteArray &buffer) noexcept override {
        return check(buffer);
      }
      bool check(QByteArray &buffer) noexcept override {
        return (buffer.size()>=sizeof(T));
      }
      T result(QByteArray &buffer) override {
        T value=0;
        memcpy(&value, buffer.data(), sizeof(T));
        buffer=buffer.mid(sizeof(T));
        return value;
      }
      const char *name() override { return "CorralQIODevice::ReadInt"; }
  };
  class ReadLinesAwaited: public NonCancellableAwaited<bool> {
  public:
    ReadLinesAwaited(CorralQIODevice device, std::function<bool(QString)> consume): NonCancellableAwaited<bool>{device}, m_consume(consume) {}
    bool initialCheck(QByteArray &buffer) noexcept override {
      m_startIndex=m_curIndex=0;
      return consumeData(buffer);
    }
    bool check(QByteArray &buffer) noexcept override {
      return consumeData(buffer);
    }
    bool result(QByteArray &buffer) override { return m_result; }
    const char *name() override { return "CorralQIODevice::ReadLines"; }
  protected:
    bool consumeData(QByteArray &buffer);
    std::function<bool(QString)> m_consume;
    bool m_result=false;
    char m_lastNewLine='\0';
    size_t m_startIndex=0;
    size_t m_curIndex=0;
  };

public:
  ReadAwaited read(size_t n, bool partial=true) { return ReadAwaited(*this, n, partial); }
  template <typename T> ReadIntAwaited<T> readInt() {
    return ReadIntAwaited<T>(*this);
  }
  ReadIntAwaited<char> readChar() {
      return ReadIntAwaited<char>(*this);
  }
  ReadLinesAwaited readLines(std::function<bool(QString)> consume) { return ReadLinesAwaited(*this, consume); }
  DiscardAwaited discard() { return DiscardAwaited(*this); }
};
