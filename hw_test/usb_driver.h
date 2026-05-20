#pragma once
#ifndef USB_DRIVER_H
#define USB_DRIVER_H

#include <cstdint>
#include <string>
#include <vector>

namespace hw_test {

// VLFD USB device driver wrapper
// Wraps vlfd-ffi C interface with object-oriented RAM test operations
class UsbDriver {
public:
    UsbDriver();
    ~UsbDriver();

    UsbDriver(const UsbDriver&) = delete;
    UsbDriver& operator=(const UsbDriver&) = delete;

    bool programFpga(const std::string& bitfile_path);
    bool open();
    void close();
    bool isOpen() const { return device_ != nullptr; }

    // Single write/read: send 64-bit data, read back 64-bit data
    uint64_t writeRead(uint64_t write_data);

    // Batch write/read
    std::vector<uint64_t> writeReadBatch(const std::vector<uint64_t>& write_data_list);

    std::string getLastError() const;

    // Set USB cycle period (ms), default 5ms (200Hz)
    void setCyclePeriodMs(int ms) { cycle_period_ms_ = ms; }

private:
    void* device_;  // opaque pointer to VlfdDevice
    int cycle_period_ms_;

    static void splitUint64(uint64_t value, uint16_t buf[4]);
    static uint64_t mergeUint64(const uint16_t buf[4]);
};

} // namespace hw_test

#endif // USB_DRIVER_H
