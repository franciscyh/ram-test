#include "usb_driver.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "../../Rabbit/vlfd-ffi/vlfd_ffi.h"
}

namespace hw_test {

UsbDriver::UsbDriver() : device_(nullptr), cycle_period_ms_(5) {}

UsbDriver::~UsbDriver() {
    if (device_) {
        close();
    }
}

bool UsbDriver::programFpga(const std::string& bitfile_path) {
    int result = vlfd_program_fpga(bitfile_path.c_str());
    if (result != 0) {
        fprintf(stderr, "[ERROR] Failed to program FPGA: %s\n", vlfd_get_last_error_message());
        return false;
    }
    printf("[OK] FPGA programmed: %s\n", bitfile_path.c_str());
    return true;
}

bool UsbDriver::open() {
    if (device_) {
        return true;
    }
    device_ = static_cast<void*>(vlfd_io_open());
    if (!device_) {
        fprintf(stderr, "[ERROR] Failed to open USB device: %s\n", vlfd_get_last_error_message());
        return false;
    }
    printf("[OK] USB device opened\n");
    return true;
}

void UsbDriver::close() {
    if (device_) {
        vlfd_io_close(static_cast<VlfdDevice*>(device_));
        device_ = nullptr;
        printf("[OK] USB device closed\n");
    }
}

uint64_t UsbDriver::writeRead(uint64_t write_data) {
    if (!device_) {
        fprintf(stderr, "[ERROR] Device not open\n");
        return 0;
    }

    uint16_t write_buf[4] = {0};
    uint16_t read_buf[4] = {0};

    splitUint64(write_data, write_buf);

    int result = vlfd_io_write_read(static_cast<VlfdDevice*>(device_), write_buf, read_buf, 4);
    if (result != 0) {
        fprintf(stderr, "[ERROR] USB write/read failed: %s\n", vlfd_get_last_error_message());
        return 0;
    }

    // Wait one cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(cycle_period_ms_));

    return mergeUint64(read_buf);
}

std::vector<uint64_t> UsbDriver::writeReadBatch(const std::vector<uint64_t>& write_data_list) {
    std::vector<uint64_t> read_data_list;
    read_data_list.reserve(write_data_list.size());

    for (uint64_t wdata : write_data_list) {
        read_data_list.push_back(writeRead(wdata));
    }

    return read_data_list;
}

std::string UsbDriver::getLastError() const {
    const char* msg = vlfd_get_last_error_message();
    return msg ? std::string(msg) : "";
}

void UsbDriver::splitUint64(uint64_t value, uint16_t buf[4]) {
    buf[0] = static_cast<uint16_t>(value & 0xFFFF);
    buf[1] = static_cast<uint16_t>((value >> 16) & 0xFFFF);
    buf[2] = static_cast<uint16_t>((value >> 32) & 0xFFFF);
    buf[3] = static_cast<uint16_t>((value >> 48) & 0xFFFF);
}

uint64_t UsbDriver::mergeUint64(const uint16_t buf[4]) {
    return (static_cast<uint64_t>(buf[3]) << 48) |
           (static_cast<uint64_t>(buf[2]) << 32) |
           (static_cast<uint64_t>(buf[1]) << 16) |
           static_cast<uint64_t>(buf[0]);
}

} // namespace hw_test
