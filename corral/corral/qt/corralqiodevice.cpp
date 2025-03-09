#include "corralqiodevice.h"
#include <QDebug>
#include <QDateTime>
#include <QFile>
CorralQIODevice CorralQIODevice::create(QIODevice *device, QString name, bool debug)
{
  return CorralQIODevice(device, name, debug);
}

CorralQIODevice CorralQIODevice::open(QIODevice *device, QString name,
                                      QIODeviceBase::OpenMode mode, bool debug) {
  CorralQIODevice ret;
  if (device && device->open(mode))
    ret = create(device, name, debug);
  else
    delete device;
  return ret;
}

CorralQIODevice CorralQIODevice::open(QIODevice *device, QString name, bool debug) {
  return open(device, name, QIODevice::ReadWrite, debug);
}

CorralQIODevice CorralQIODevice::open(QIODevice *device, QString name) {
  return open(device, name, QIODevice::ReadWrite, false);
}

CorralQIODevice::CorralQIODevice(QIODevice *device, QString name, bool debug): m_instance(std::shared_ptr<CorralQIODeviceInstance>(new CorralQIODeviceInstance(device, name, debug))) {}

bool CorralQIODevice::writeChar(char ch) {
  return m_instance?m_instance->write(QByteArray(&ch, 1)):false;
  // bool ret=false;
  // if(auto d=device()) {
  //   d->write(&ch, 1);
  //   ret=d->waitForBytesWritten(500);
  // }
  // return ret;
}

bool CorralQIODevice::write(const QByteArray &data) {
  return m_instance?m_instance->write(data):false;
  // bool ret=false;
  // if(auto d=device()) {
  //   d->write(data);
  //   ret=d->waitForBytesWritten(500);
  // }
  // return ret;
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

CorralQIODeviceInstance::~CorralQIODeviceInstance() {}

bool CorralQIODeviceInstance::write(const QByteArray &data) {
  bool ret=false;
  if(m_device) {
    m_device->write(data);
    ret=m_device->waitForBytesWritten(500);
    if(m_debugOutput) {
      printDebug(QString::fromLatin1("> ")+byteArrayToString(data));
    }
  }
  return ret;
}

QString CorralQIODeviceInstance::byteArrayToString(const QByteArray &data) {
  QString ret="'";
  for(char ch: data) {
    if(ch=='\\')
      ret+="\\\\";
    else if(ch=='\n')
      ret+="\\n";
    else if(ch=='\r')
      ret+="\\r";
    else if(ch=='\b')
      ret+="\\b";
    else if(ch=='\"')
      ret+="\\\"";
    else if(ch=='\'')
      ret+="\\\'";
    else if(std::isprint(ch))
      ret+=ch;
    else
      ret+=QString("\\x%1").arg((unsigned)ch, 2, 16, QChar('0'));
  }
  ret+=QString::fromLatin1("' %1").arg(QString::fromLatin1(data.toHex()));
  return ret;
}

CorralQIODeviceInstance::CorralQIODeviceInstance(QIODevice *device, QString name, bool debug)
    : m_device(device) {
  if (debug) {
    m_debugOutput = std::make_unique<QFile>(name+".log");
    if (m_debugOutput->open(QIODevice::Text | QIODevice::Append))
      printDebug(QString::fromLatin1("New session for %1").arg(name));
    else
      m_debugOutput=nullptr;
  }
}

void CorralQIODeviceInstance::printDebug(QString message) {
  if(m_debugOutput) {
    m_debugOutput->write(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss.zzz | ").toUtf8());
    m_debugOutput->write(message.toUtf8());
    m_debugOutput->write("\n");
    m_debugOutput->flush();
  }
}
