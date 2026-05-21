#pragma once
#ifndef RAM_TEST_CASES_H
#define RAM_TEST_CASES_H

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace hw_test {

class PinMapping;
class UsbDriver;

struct TestError {
    int test_id;
    int addr;
    uint32_t expected;
    uint32_t actual;
    std::string desc;
};

struct TestResult {
    std::string test_name;
    int total_checks;
    int error_count;
    std::vector<TestError> errors;
    double elapsed_ms;

    bool passed() const { return error_count == 0; }
};

// RAM hardware test executor
// Encapsulates 4 test strategies matching tb_test.sv logic
class RamTestExecutor {
public:
    RamTestExecutor(PinMapping& pin_map, UsbDriver& usb, int width, int depth,
                     int width_b = 0, int depth_b = 0);

    std::vector<TestResult> runAllTests();

    TestResult test1SequentialWriteRead();
    TestResult test2ReverseWriteRead();
    TestResult test3StrideAccess();
    TestResult test4RandomAccess();
    TestResult test5PortAWritePortBRead();
    TestResult test6PortBWritePortARead();

    void setPrintInterval(int n) { print_interval_ = n; }
    void setFrequency(int freq);
    void setDualPort(bool enable);

private:
    PinMapping& pin_map_;
    UsbDriver& usb_;
    int width_;
    int depth_;
    int addr_width_;
    int print_interval_;
    int width_b_;
    int depth_b_;
    int addr_width_b_;
    bool is_dual_port_ = false;

    uint64_t buildWriteData(int ena, int wea, int addr, uint32_t data_in);
    uint64_t buildWriteDataAB(int ena, int wea, int addr_a, uint32_t data_in_a,
                               int enb, int web, int addr_b, uint32_t data_in_b);
    uint32_t extractReadData(uint64_t read_data);
    uint32_t extractReadDataA(uint64_t read_data);
    uint32_t extractReadDataB(uint64_t read_data);
    void ramWrite(int addr, uint32_t data);
    uint32_t ramRead(int addr);
    void ramWriteA(int addr, uint32_t data);
    void ramWriteB(int addr, uint32_t data);
    uint32_t ramReadA(int addr);
    uint32_t ramReadB(int addr);
    void reportError(TestResult& result, int test_id, int addr,
                     uint32_t expected, uint32_t actual, const char* desc);
    bool hasPortB() const;
    bool isSymmetricDualPort() const;
};

} // namespace hw_test

#endif // RAM_TEST_CASES_H
