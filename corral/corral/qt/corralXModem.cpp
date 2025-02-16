#include "corralXModem.h"

#include <QtEndian>
#include "corralqiodevice.h"
#include "corralqt.h"

namespace XModem {
using namespace std::chrono_literals;

enum class Ctrl : char {
    SOH = 0x01, ///< @brief Start Of Header
    EOT = 0x04, ///< @brief End Of Transmission
    ACK = 0x06, ///< @brief ACKnowledge
    NAK = 0x15, ///< @brief Not AcKnowledge
    CAN = 0x18, ///< @brief CANcel transmission
    NCG = 0x43  ///< @brief Start transfer
};

/**
 * @brief Crc16, xmodem
 * @param ptr Pointer to data
 * @param size Size of the data
 * @return Crc of the data
 */
static uint16_t crc16(const void *ptr, unsigned int size)
{
    uint16_t crc = 0x0;
    const uint8_t *data = (const uint8_t *) ptr;

    for (unsigned i = 0; i < size; i++) {
        crc = (crc >> 8) | (crc << 8);
        crc ^= *data++;
        crc ^= (crc & 0xff) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xff) << 5;
    }
    return crc;
}

corral::Task<bool> upload(CorralQIODevice device, std::function<corral::Task<std::variant<QByteArray, bool>>(uint32_t)> dataFunction)
{
    bool ret = false;
    auto start = co_await qAwaitTimeout(5s, device.readChar());
    if (start.value_or('\0') == static_cast<char>(XModem::Ctrl::NCG) && dataFunction) {
        ret = true;
        unsigned sequenceNumber = 1;
        while (ret) {
            auto dataOrResult = co_await dataFunction(sequenceNumber - 1);
            if(dataOrResult.index()==1) {
              ret=std::get<1>(dataOrResult);
              break;
            }
            auto data=std::get<0>(dataOrResult);
            if (data.size() != BLOCK_SIZE) {
              ret = false;
              break;
            }
            uint16_t crc = crc16(data.data(), data.size());
            // Prepares the data for current block
            QByteArray writeData(sizeof(uint8_t) + BLOCK_SIZE + 2 * sizeof(uint16_t), '0');
            writeData[0] = static_cast<uint8_t>(Ctrl::SOH);
            writeData[1] = static_cast<uint8_t>(sequenceNumber & 0xFFu);
            writeData[2] = static_cast<uint8_t>(~(sequenceNumber & 0xFFu));
            memcpy(&writeData[1 + sizeof(uint16_t)], data.data(), BLOCK_SIZE);
            qToBigEndian<uint16_t>(crc, &writeData[1 + BLOCK_SIZE + sizeof(uint16_t)]);
            for (unsigned transmitCount = 0; ret; transmitCount++) {
                bool retry = false;
                device.write(writeData);
                auto reply = co_await qAwaitTimeout(10s, device.readChar());
                if (!reply) // Timeout
                    ret = false;
                else {
                    char replyChar = *reply;
                    if (replyChar == static_cast<char>(Ctrl::ACK))
                        // Proceed
                        sequenceNumber++;
                    else if (replyChar == static_cast<char>(Ctrl::NCG)) {
                        // Ignores subsequent NCG (probably we took too much time to get first block)
                    }
                    else if (replyChar == static_cast<char>(Ctrl::CAN)) {
                      // Cancel request from remote side
                      ret=false;
                    } else if (transmitCount
                               < 4) { // Everything else is considered as a NAK. Retries.
                        transmitCount++;
                        qDebug()<<"About to retry"<<transmitCount<<ret;
                        retry = true;
                    } else // Maximum number of re-transmission exceeded
                        ret = false;
                }
                if (!retry)
                    break;
            }
        }
        if (ret) {
            device.writeChar(static_cast<char>(
                XModem::Ctrl::EOT)); // Sends end of transmission and waits for ACK
            auto reply = co_await qAwaitTimeout(10s, device.readChar());
            ret = (reply.value_or('\0') == static_cast<char>(XModem::Ctrl::ACK));
        }
        if (!ret) {
            // Sends the end of transfer
            for (unsigned i = 0; i < 4; i++)
                device.writeChar(static_cast<char>(XModem::Ctrl::CAN));
        }
    }
    co_return ret;
}

corral::Task<bool> download(
    CorralQIODevice device,
    std::function<corral::Task<bool>(QByteArray)> dataFunction)
{
  bool ret=false;
  char command='\0';
  {
    QElapsedTimer t;
    t.start();
    while(t.elapsed()<10000) {
      device.writeChar(static_cast<char>(XModem::Ctrl::NCG));
      auto reply = co_await qAwaitTimeout(500ms, device.readChar());
      if(reply) {
        command=*reply;
        ret=true;
        break;
      }
    }
  }
  if(ret) {
    uint32_t sequenceNumber = 1;
    bool exitLoop = false;  // Exit the receive loop
    bool sendReply = false;  // Sends a reply to remote side (ACK if result is true, CAN if result is false)
    /* Start receiving XMODEM packets */
    while (!exitLoop) {
        switch (command) {
        case static_cast<char>(XModem::Ctrl::SOH):
            // Request for start of transmission. We can start requesting packets.
            break;
        case static_cast<char>(XModem::Ctrl::EOT):
            // We finished the reception successfully.
            exitLoop = sendReply = true;  // Should exit the loop and send ACK reply
            break;
        case static_cast<char>(XModem::Ctrl::CAN):
            // Cancel transmission
            exitLoop = true; ret=false; // We should just exit the loop. No need to send reply.
            break;
        default: // Invalid command
            ret=false; exitLoop = sendReply = true; // Should exit the loop and send CAN reply.
            break;
        }
        if (!exitLoop) {
            static constexpr uint32_t READ_SIZE=BLOCK_SIZE + 2 * sizeof(uint16_t);
            auto data = co_await qAwaitTimeout(5000ms, device.read(READ_SIZE));
            if(data && data->size()==READ_SIZE) {
              uint8_t sequence=(uint8_t)(*data)[0], negSequence=(uint8_t)(*data)[1];
              uint16_t crc=qFromBigEndian<uint16_t>(&(*data)[BLOCK_SIZE + sizeof(uint16_t)]);
              ret=ret && (sequence==static_cast<uint8_t>(sequenceNumber&0xFFu));
              ret=ret && (negSequence==static_cast<uint8_t>(~(sequenceNumber&0xFFu)));
              ret=ret && (crc==crc16(&(*data)[2], BLOCK_SIZE));
            }
            else {
              sendReply=true;
              exitLoop=true;
              ret=false;
              break;
            }
            if(exitLoop) {

            }
            else if(!ret)
                device.writeChar(static_cast<char>(XModem::Ctrl::NAK));
            else {
              ret=co_await dataFunction(data->mid(2, BLOCK_SIZE));
              if(!ret)
                exitLoop=sendReply=true;
              else {
                device.writeChar(static_cast<char>(XModem::Ctrl::ACK));
                sequenceNumber++;
              }
            }
            if(!exitLoop) {
                // If it's confirmed we don't have to exit the loop read the next command.
                auto reply = co_await qAwaitTimeout(500ms, device.readChar());
                if(reply)
                  command=*reply;
                else {
                  sendReply=true;
                  ret=false;
                  exitLoop=true;
                }
            }
        }
    }
    // Terminates the transfer
    if (sendReply) {
        if (ret)
            device.writeChar(static_cast<char>(XModem::Ctrl::ACK));  // Sends an ACK
        else
            for (unsigned i = 0; i < 4;
                 i++) { // Sends more than one CAN (makes sure remote ends receives it)
                device.writeChar(static_cast<char>(XModem::Ctrl::CAN));
                co_await qSleepFor(50ms);
            }
    }
  }
  co_return ret;
}

corral::Task<bool> upload(CorralQIODevice device, const QByteArray &data, const std::function<void(uint32_t sizeWritten)> &callback) {
  return upload(device, [data, callback](uint32_t block)->corral::Task<std::variant<QByteArray, bool>> {
    uint32_t offset=block*128u;
    if(callback)
      callback(offset);
    if(offset>=data.size()) {
      co_return true;
    }
    auto cur=data.mid(offset, 128);
    if(cur.size()<128) cur.append(QByteArray(128-cur.size(), 0xFF));
    co_return cur;
  });
}
}
