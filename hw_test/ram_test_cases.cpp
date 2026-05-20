#include "ram_test_cases.h"
#include "pin_mapping.h"
#include "usb_driver.h"
#include "test_patterns.h"
#include <cmath>
#include <cstdio>
#include <chrono>

namespace hw_test {

RamTestExecutor::RamTestExecutor(PinMapping& pin_map, UsbDriver& usb, int width, int depth)
    : pin_map_(pin_map), usb_(usb), width_(width), depth_(depth),
      addr_width_(static_cast<int>(std::ceil(std::log2(depth)))), print_interval_(100) {}

void RamTestExecutor::setFrequency(int freq) {
    if (freq > 0) {
        usb_.setCyclePeriodMs(1000 / freq);
    }
}

uint64_t RamTestExecutor::buildWriteData(int ena, int wea, int addr, uint32_t data_in) {
    uint64_t wdata = 0;

    int ena_pos = pin_map_.getBitPosition("ena");
    if (ena_pos >= 0) wdata |= (static_cast<uint64_t>(ena & 1) << ena_pos);

    int wea_pos = pin_map_.getBitPosition("wea");
    if (wea_pos >= 0) wdata |= (static_cast<uint64_t>(wea & 1) << wea_pos);

    auto addr_bits = pin_map_.getSignalBits("addr_a");
    for (size_t i = 0; i < addr_bits.size(); ++i) {
        int bit_val = (addr >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << addr_bits[i]->bit_position);
    }

    auto data_in_bits = pin_map_.getSignalBits("data_in_a");
    for (size_t i = 0; i < data_in_bits.size(); ++i) {
        int bit_val = (data_in >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << data_in_bits[i]->bit_position);
    }

    return wdata;
}

uint32_t RamTestExecutor::extractReadData(uint64_t read_data) {
    uint32_t data = 0;
    auto data_out_bits = pin_map_.getSignalBits("data_out_a");
    for (size_t i = 0; i < data_out_bits.size(); ++i) {
        int pos = data_out_bits[i]->bit_position;
        if (pos >= 0) {
            data |= static_cast<uint32_t>(((read_data >> pos) & 1) << i);
        }
    }
    return data;
}

void RamTestExecutor::ramWrite(int addr, uint32_t data) {
    uint64_t wdata = buildWriteData(1, 1, addr, data);
    usb_.writeRead(wdata);
}

uint32_t RamTestExecutor::ramRead(int addr) {
    uint64_t wdata = buildWriteData(1, 0, addr, 0);
    usb_.writeRead(wdata);  // First cycle: FPGA latches address
    uint64_t rdata = usb_.writeRead(wdata);  // Second cycle: read data (Block RAM 1-cycle latency)
    return extractReadData(rdata);
}

void RamTestExecutor::reportError(TestResult& result, int test_id, int addr,
                                   uint32_t expected, uint32_t actual, const char* desc) {
    TestError err;
    err.test_id = test_id;
    err.addr = addr;
    err.expected = expected;
    err.actual = actual;
    err.desc = desc;
    result.errors.push_back(err);
    result.error_count++;
}

// ============================================================================
// Test 1: Sequential write all addresses -> sequential read verify
// ============================================================================

TestResult RamTestExecutor::test1SequentialWriteRead() {
    TestResult result;
    result.test_name = "Sequential Write/Read";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    ReferenceMemory ref(depth_, width_);

    printf("\n[TEST 1] Sequential write all addresses (depth=%d, width=%d)...\n", depth_, width_);

    for (int addr = 0; addr < depth_; ++addr) {
        uint32_t data = testPattern(addr, width_);
        ramWrite(addr, data);
        ref.write(addr, data);

        if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
            printf("  Write progress: %d/%d\r", addr + 1, depth_);
            fflush(stdout);
        }
    }
    printf("  Write progress: %d/%d  [OK]\n", depth_, depth_);

    // Disable write before reading
    uint64_t wdata = buildWriteData(0, 0, 0, 0);
    usb_.writeRead(wdata);
    usb_.writeRead(wdata);

    printf("[TEST 1] Sequential read all addresses...\n");

    for (int addr = 0; addr < depth_; ++addr) {
        uint32_t actual = ramRead(addr);
        uint32_t expected = ref.read(addr);
        result.total_checks++;

        if (actual != expected) {
            reportError(result, 1, addr, expected, actual, "Sequential read mismatch");
        }

        if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
            printf("  Read progress: %d/%d (errors=%d)\r", addr + 1, depth_, result.error_count);
            fflush(stdout);
        }
    }
    printf("  Read progress: %d/%d (errors=%d)  [%s]\n",
           depth_, depth_, result.error_count, result.passed() ? "PASS" : "FAIL");

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

// ============================================================================
// Test 2: Reverse order write -> reverse order read
// ============================================================================

TestResult RamTestExecutor::test2ReverseWriteRead() {
    TestResult result;
    result.test_name = "Reverse Write/Read";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    ReferenceMemory ref(depth_, width_);

    printf("\n[TEST 2] Reverse write all addresses...\n");

    for (int addr = depth_ - 1; addr >= 0; --addr) {
        uint32_t data = testPatternInv(addr, width_);
        ramWrite(addr, data);
        ref.write(addr, data);

        if (print_interval_ > 0 && (depth_ - addr) % print_interval_ == 0) {
            printf("  Write progress: %d/%d\r", depth_ - addr, depth_);
            fflush(stdout);
        }
    }
    printf("  Write progress: %d/%d  [OK]\n", depth_, depth_);

    uint64_t wdata = buildWriteData(0, 0, 0, 0);
    usb_.writeRead(wdata);
    usb_.writeRead(wdata);

    printf("[TEST 2] Reverse read all addresses...\n");

    for (int addr = depth_ - 1; addr >= 0; --addr) {
        uint32_t actual = ramRead(addr);
        uint32_t expected = ref.read(addr);
        result.total_checks++;

        if (actual != expected) {
            reportError(result, 2, addr, expected, actual, "Reverse read mismatch");
        }

        if (print_interval_ > 0 && (depth_ - addr) % print_interval_ == 0) {
            printf("  Read progress: %d/%d (errors=%d)\r", depth_ - addr, depth_, result.error_count);
            fflush(stdout);
        }
    }
    printf("  Read progress: %d/%d (errors=%d)  [%s]\n",
           depth_, depth_, result.error_count, result.passed() ? "PASS" : "FAIL");

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

// ============================================================================
// Test 3: Stride access
// ============================================================================

TestResult RamTestExecutor::test3StrideAccess() {
    TestResult result;
    result.test_name = "Stride Access";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    printf("\n[TEST 3] Stride access...\n");

    for (int stride = 1; stride < depth_; stride *= 2) {
        ReferenceMemory ref(depth_, width_);

        for (int addr = 0; addr < depth_; addr += stride) {
            uint32_t data = testPatternStride(addr, stride, width_);
            ramWrite(addr, data);
            ref.write(addr, data);
        }

        uint64_t wdata = buildWriteData(0, 0, 0, 0);
        usb_.writeRead(wdata);
        usb_.writeRead(wdata);

        for (int addr = 0; addr < depth_; addr += stride) {
            uint32_t actual = ramRead(addr);
            uint32_t expected = ref.read(addr);
            result.total_checks++;

            if (actual != expected) {
                char desc[128];
                snprintf(desc, sizeof(desc), "Stride=%d read mismatch", stride);
                reportError(result, 3, addr, expected, actual, desc);
            }
        }

        printf("  Stride=%d: checked %d addresses, errors=%d\n",
               stride, (depth_ + stride - 1) / stride, result.error_count);
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    printf("[TEST 3] Stride access  [%s]\n", result.passed() ? "PASS" : "FAIL");

    return result;
}

// ============================================================================
// Test 4: Random access
// ============================================================================

TestResult RamTestExecutor::test4RandomAccess() {
    TestResult result;
    result.test_name = "Random Access";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    ReferenceMemory ref(depth_, width_);

    printf("\n[TEST 4] Clearing RAM before random test...\n");
    for (int addr = 0; addr < depth_; ++addr) {
        ramWrite(addr, 0);
        if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
            printf("  Clear progress: %d/%d\r", addr + 1, depth_);
            fflush(stdout);
        }
    }
    printf("  Clear progress: %d/%d  [OK]\n", depth_, depth_);

    // Disable write before reading back
    uint64_t idle_data = buildWriteData(0, 0, 0, 0);
    usb_.writeRead(idle_data);
    usb_.writeRead(idle_data);

    printf("[TEST 4] Random write/read...\n");

    int num_tests = (depth_ * 2 < 10000) ? depth_ * 2 : 10000;
    srand(42);

    for (int i = 0; i < num_tests; ++i) {
        int addr = rand() % depth_;
        uint32_t data = rand() & widthMask(width_);
        ramWrite(addr, data);
        ref.write(addr, data);
    }

    uint64_t wdata = buildWriteData(0, 0, 0, 0);
    usb_.writeRead(wdata);
    usb_.writeRead(wdata);

    srand(42);
    for (int i = 0; i < num_tests; ++i) {
        int addr = rand() % depth_;
        uint32_t actual = ramRead(addr);
        uint32_t expected = ref.read(addr);
        result.total_checks++;

        if (actual != expected) {
            reportError(result, 4, addr, expected, actual, "Random read mismatch");
        }

        if (print_interval_ > 0 && (i + 1) % print_interval_ == 0) {
            printf("  Progress: %d/%d (errors=%d)\r", i + 1, num_tests, result.error_count);
            fflush(stdout);
        }
    }
    printf("  Progress: %d/%d (errors=%d)  [%s]\n",
           num_tests, num_tests, result.error_count, result.passed() ? "PASS" : "FAIL");

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

// ============================================================================
// Run all tests
// ============================================================================

std::vector<TestResult> RamTestExecutor::runAllTests() {
    std::vector<TestResult> results;

    results.push_back(test1SequentialWriteRead());
    results.push_back(test2ReverseWriteRead());
    results.push_back(test3StrideAccess());
    results.push_back(test4RandomAccess());

    return results;
}

} // namespace hw_test
