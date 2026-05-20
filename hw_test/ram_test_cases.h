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
    RamTestExecutor(PinMapping& pin_map, UsbDriver& usb, int width, int depth);

    std::vector<TestResult> runAllTests();

    TestResult test1SequentialWriteRead();
    TestResult test2ReverseWriteRead();
    TestResult test3StrideAccess();
    TestResult test4RandomAccess();

    void setPrintInterval(int n) { print_interval_ = n; }
    void setFrequency(int freq);

private:
    PinMapping& pin_map_;
    UsbDriver& usb_;
    int width_;
    int depth_;
    int addr_width_;
    int print_interval_;

    uint64_t buildWriteData(int ena, int wea, int addr, uint32_t data_in);
    uint32_t extractReadData(uint64_t read_data);
    void ramWrite(int addr, uint32_t data);
    uint32_t ramRead(int addr);
    void reportError(TestResult& result, int test_id, int addr,
                     uint32_t expected, uint32_t actual, const char* desc);
};

} // namespace hw_test

#endif // RAM_TEST_CASES_H
