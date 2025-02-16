#pragma once
#include "corralqiodevice.h"
#include <variant>
namespace XModem {

static constexpr unsigned BLOCK_SIZE = 128;

/**
 * @brief Generic upload of data with xmodem protocol
 * @param The device to use
 * @param A function returning the data to write. Should be a functor that given a block (0-index) will return a byte array with the 128 to write, false on failure or true on completion.
 * @return True on success
 */
corral::Task<bool> upload(CorralQIODevice device, std::function<corral::Task<std::variant<QByteArray, bool>>(uint32_t)> dataFunction);
/**
 * @brief Simplified version of upload
 * @param device The device to use
 * @param data The bytes to write to the device. If not aligned to 128 bytes will be padded with zeroes
 * @return True on success
 */
corral::Task<bool> upload(CorralQIODevice device, const QByteArray &data,
                          const std::function<void(uint32_t)> &callback={});

/**
 * @brief Download a file
 */
corral::Task<bool> download(CorralQIODevice device, std::function<corral::Task<bool>(QByteArray data)> dataFunction);

} // namespace XModem
