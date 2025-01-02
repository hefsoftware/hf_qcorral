#include "corralqiodevice.h"
#include <QDebug>

CorralQIODevice CorralQIODevice::create(QIODevice *device)
{
  return CorralQIODevice(device);
}

CorralQIODevice CorralQIODevice::open(QIODevice *device, QIODeviceBase::OpenMode mode)
{
  CorralQIODevice ret;
  if (device && device->open(mode))
    ret = create(device);
  else
    delete device;
  return ret;
}

CorralQIODevice::CorralQIODevice(QIODevice *device): m_instance(std::shared_ptr<CorralQIODeviceInstance>(new CorralQIODeviceInstance(device))) {}

bool CorralQIODevice::writeChar(char ch) {
  bool ret=false;
  if(auto d=device()) {
    d->write(&ch, 1);
    ret=d->waitForBytesWritten(500);
  }
  return ret;
}

bool CorralQIODevice::write(const QByteArray &data) {
  bool ret=false;
  if(auto d=device()) {
    d->write(data);
    ret=d->waitForBytesWritten(500);
  }
  return ret;
}

QIODevice *CorralQIODevice::device() {
  return m_instance?m_instance->m_device.get():nullptr;
}

bool CorralQIODevice::ReadAwaited::initialCheck(QByteArray &buffer) noexcept {
  return (buffer.size()>=m_n);
}

bool CorralQIODevice::ReadAwaited::check(QByteArray &buffer) noexcept
{
  return (buffer.size() >= m_n);
}

QByteArray CorralQIODevice::ReadAwaited::result(QByteArray &buffer) {
  auto ret=buffer.left(m_n);
  buffer=buffer.mid(m_n);
  return ret;
}

bool CorralQIODevice::ReadLinesAwaited::consumeData(QByteArray &buffer) {
  bool ret=false;
  for(;!ret && m_curIndex<buffer.size();m_curIndex++) {
    auto cur=buffer[m_curIndex];
    if(cur=='\n' || cur=='\r') {
      if(m_lastNewLine!='\0' && cur!=m_lastNewLine) {
        // Skip \r after \n and vice versa
        m_lastNewLine='\0';
        m_startIndex++;
        continue;
      }
      m_lastNewLine=cur;
      ret=(m_consume && m_consume(QString::fromUtf8(QByteArrayView(buffer).mid(m_startIndex, m_curIndex-m_startIndex))));
      m_startIndex=m_curIndex+1;
    }
    else
      m_lastNewLine='\0';
  }
  buffer=buffer.mid(m_startIndex);
  m_curIndex-=m_startIndex;
  m_startIndex=0;
  return ret;
}
