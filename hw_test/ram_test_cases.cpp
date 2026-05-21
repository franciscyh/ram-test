#include "ram_test_cases.h"
#include "pin_mapping.h"
#include "usb_driver.h"
#include "test_patterns.h"
#include <cmath>
#include <cstdio>
#include <chrono>

namespace hw_test {

RamTestExecutor::RamTestExecutor(PinMapping& pin_map, UsbDriver& usb, int width, int depth,
                                     int width_b, int depth_b)
    : pin_map_(pin_map), usb_(usb), width_(width), depth_(depth),
      addr_width_(static_cast<int>(std::ceil(std::log2(depth)))),
      print_interval_(100),
      width_b_(width_b), depth_b_(depth_b),
      addr_width_b_(depth_b > 0 ? static_cast<int>(std::ceil(std::log2(depth_b))) : 0) {}

void RamTestExecutor::setFrequency(int freq) {
    if (freq > 0) {
        usb_.setCyclePeriodMs(1000 / freq);
    }
}

void RamTestExecutor::setDualPort(bool enable) {
    if (enable && !hasPortB()) {
        printf("[WARN] Dual-port mode requested but no Port B signals found in pin mapping.\n");
        printf("       Falling back to single-port mode.\n");
        is_dual_port_ = false;
    } else if (enable && (width_b_ == 0 || depth_b_ == 0)) {
        printf("[WARN] Dual-port mode requested but B-port config is missing (width_b=%d, depth_b=%d).\n",
               width_b_, depth_b_);
        printf("       Use a config dir like '4x2048_4x2048' or '4x2048_8x1024'.\n");
        is_dual_port_ = false;
    } else {
        is_dual_port_ = enable;
    }
}

bool RamTestExecutor::hasPortB() const {
    return pin_map_.getBitPosition("enb") >= 0;
}

bool RamTestExecutor::isSymmetricDualPort() const {
    return hasPortB() && width_ == width_b_ && depth_ == depth_b_;
}

uint64_t RamTestExecutor::buildWriteData(int ena, int wea, int addr, uint32_t data_in) {
    return buildWriteDataAB(ena, wea, addr, data_in, 0, 0, 0, 0);
}

uint64_t RamTestExecutor::buildWriteDataAB(int ena, int wea, int addr_a, uint32_t data_in_a,
                                            int enb, int web, int addr_b, uint32_t data_in_b) {
    uint64_t wdata = 0;

    // Port A
    int ena_pos = pin_map_.getBitPosition("ena");
    if (ena_pos >= 0) wdata |= (static_cast<uint64_t>(ena & 1) << ena_pos);

    int wea_pos = pin_map_.getBitPosition("wea");
    if (wea_pos >= 0) wdata |= (static_cast<uint64_t>(wea & 1) << wea_pos);

    auto addr_a_bits = pin_map_.getSignalBits("addr_a");
    for (size_t i = 0; i < addr_a_bits.size(); ++i) {
        int bit_val = (addr_a >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << addr_a_bits[i]->bit_position);
    }

    auto data_in_a_bits = pin_map_.getSignalBits("data_in_a");
    for (size_t i = 0; i < data_in_a_bits.size(); ++i) {
        int bit_val = (data_in_a >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << data_in_a_bits[i]->bit_position);
    }

    // Port B
    int enb_pos = pin_map_.getBitPosition("enb");
    if (enb_pos >= 0) wdata |= (static_cast<uint64_t>(enb & 1) << enb_pos);

    int web_pos = pin_map_.getBitPosition("web");
    if (web_pos >= 0) wdata |= (static_cast<uint64_t>(web & 1) << web_pos);

    auto addr_b_bits = pin_map_.getSignalBits("addr_b");
    for (size_t i = 0; i < addr_b_bits.size(); ++i) {
        int bit_val = (addr_b >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << addr_b_bits[i]->bit_position);
    }

    auto data_in_b_bits = pin_map_.getSignalBits("data_in_b");
    for (size_t i = 0; i < data_in_b_bits.size(); ++i) {
        int bit_val = (data_in_b >> static_cast<int>(i)) & 1;
        wdata |= (static_cast<uint64_t>(bit_val) << data_in_b_bits[i]->bit_position);
    }

    return wdata;
}

uint32_t RamTestExecutor::extractReadData(uint64_t read_data) {
    return extractReadDataA(read_data);
}

uint32_t RamTestExecutor::extractReadDataA(uint64_t read_data) {
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

uint32_t RamTestExecutor::extractReadDataB(uint64_t read_data) {
    uint32_t data = 0;
    auto data_out_bits = pin_map_.getSignalBits("data_out_b");
    for (size_t i = 0; i < data_out_bits.size(); ++i) {
        int pos = data_out_bits[i]->bit_position;
        if (pos >= 0) {
            data |= static_cast<uint32_t>(((read_data >> pos) & 1) << i);
        }
    }
    return data;
}

void RamTestExecutor::ramWrite(int addr, uint32_t data) {
    ramWriteA(addr, data);
}

uint32_t RamTestExecutor::ramRead(int addr) {
    return ramReadA(addr);
}

void RamTestExecutor::ramWriteA(int addr, uint32_t data) {
    uint64_t wdata = buildWriteDataAB(1, 1, addr, data, 0, 0, 0, 0);
    usb_.writeRead(wdata);
}

void RamTestExecutor::ramWriteB(int addr, uint32_t data) {
    uint64_t wdata = buildWriteDataAB(0, 0, 0, 0, 1, 1, addr, data);
    usb_.writeRead(wdata);
}

uint32_t RamTestExecutor::ramReadA(int addr) {
    uint64_t wdata = buildWriteDataAB(1, 0, addr, 0, 0, 0, 0, 0);
    usb_.writeRead(wdata);  // First cycle: FPGA latches address
    uint64_t rdata = usb_.writeRead(wdata);  // Second cycle: read data
    return extractReadDataA(rdata);
}

uint32_t RamTestExecutor::ramReadB(int addr) {
    uint64_t wdata = buildWriteDataAB(0, 0, 0, 0, 1, 0, addr, 0);
    usb_.writeRead(wdata);  // First cycle: FPGA latches address
    uint64_t rdata = usb_.writeRead(wdata);  // Second cycle: read data
    return extractReadDataB(rdata);
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
// Test 5: Cross-port A-write -> B-read (symmetric) or B self-test (asymmetric)
// ============================================================================

TestResult RamTestExecutor::test5PortAWritePortBRead() {
    TestResult result;
    result.test_name = isSymmetricDualPort() ? "Dual-Port A-Write B-Read" : "Dual-Port B Self-Test";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    if (isSymmetricDualPort()) {
        // Symmetric dual-port: strict cross-port verification
        ReferenceMemory ref(depth_, width_);

        printf("\n[TEST 5] Symmetric dual-port: Port A write -> Port B read\n");
        printf("[TEST 5] Port A sequential write all addresses (depth=%d, width=%d)...\n", depth_, width_);

        for (int addr = 0; addr < depth_; ++addr) {
            uint32_t data = testPattern(addr, width_);
            ramWriteA(addr, data);
            ref.write(addr, data);

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  A-Write progress: %d/%d\r", addr + 1, depth_);
                fflush(stdout);
            }
        }
        printf("  A-Write progress: %d/%d  [OK]\n", depth_, depth_);

        uint64_t idle = buildWriteDataAB(0, 0, 0, 0, 0, 0, 0, 0);
        usb_.writeRead(idle);
        usb_.writeRead(idle);

        printf("[TEST 5] Port B sequential read all addresses...\n");

        for (int addr = 0; addr < depth_b_; ++addr) {
            uint32_t actual = ramReadB(addr);
            uint32_t expected = ref.read(addr);
            result.total_checks++;

            if (actual != expected) {
                reportError(result, 5, addr, expected, actual, "Port B read mismatch after A write");
            }

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  B-Read progress: %d/%d (errors=%d)\r", addr + 1, depth_b_, result.error_count);
                fflush(stdout);
            }
        }
        printf("  B-Read progress: %d/%d (errors=%d)  [%s]\n",
               depth_b_, depth_b_, result.error_count, result.passed() ? "PASS" : "FAIL");
    } else {
        // Asymmetric dual-port: B-port self-test
        ReferenceMemory ref(depth_b_, width_b_);

        printf("\n[TEST 5] Asymmetric dual-port: Port B self-test (depth=%d, width=%d)\n", depth_b_, width_b_);
        printf("[TEST 5] Port B sequential write all addresses...\n");

        for (int addr = 0; addr < depth_b_; ++addr) {
            uint32_t data = testPattern(addr, width_b_);
            ramWriteB(addr, data);
            ref.write(addr, data);

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  B-Write progress: %d/%d\r", addr + 1, depth_b_);
                fflush(stdout);
            }
        }
        printf("  B-Write progress: %d/%d  [OK]\n", depth_b_, depth_b_);

        uint64_t idle = buildWriteDataAB(0, 0, 0, 0, 0, 0, 0, 0);
        usb_.writeRead(idle);
        usb_.writeRead(idle);

        printf("[TEST 5] Port B sequential read all addresses...\n");

        for (int addr = 0; addr < depth_b_; ++addr) {
            uint32_t actual = ramReadB(addr);
            uint32_t expected = ref.read(addr);
            result.total_checks++;

            if (actual != expected) {
                reportError(result, 5, addr, expected, actual, "Port B self-test read mismatch");
            }

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  B-Read progress: %d/%d (errors=%d)\r", addr + 1, depth_b_, result.error_count);
                fflush(stdout);
            }
        }
        printf("  B-Read progress: %d/%d (errors=%d)  [%s]\n",
               depth_b_, depth_b_, result.error_count, result.passed() ? "PASS" : "FAIL");
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

// ============================================================================
// Test 6: Cross-port B-write -> A-read (symmetric) or A self-test (asymmetric)
// ============================================================================

TestResult RamTestExecutor::test6PortBWritePortARead() {
    TestResult result;
    result.test_name = isSymmetricDualPort() ? "Dual-Port B-Write A-Read" : "Dual-Port A Self-Test";
    result.total_checks = 0;
    result.error_count = 0;

    auto start = std::chrono::steady_clock::now();

    if (isSymmetricDualPort()) {
        // Symmetric dual-port: strict cross-port verification
        ReferenceMemory ref(depth_, width_);

        printf("\n[TEST 6] Symmetric dual-port: Port B write -> Port A read\n");
        printf("[TEST 6] Port B reverse write all addresses (depth=%d, width=%d)...\n", depth_b_, width_b_);

        for (int addr = depth_b_ - 1; addr >= 0; --addr) {
            uint32_t data = testPatternInv(addr, width_b_);
            ramWriteB(addr, data);
            ref.write(addr, data);

            if (print_interval_ > 0 && (depth_b_ - addr) % print_interval_ == 0) {
                printf("  B-Write progress: %d/%d\r", depth_b_ - addr, depth_b_);
                fflush(stdout);
            }
        }
        printf("  B-Write progress: %d/%d  [OK]\n", depth_b_, depth_b_);

        uint64_t idle = buildWriteDataAB(0, 0, 0, 0, 0, 0, 0, 0);
        usb_.writeRead(idle);
        usb_.writeRead(idle);

        printf("[TEST 6] Port A sequential read all addresses...\n");

        for (int addr = 0; addr < depth_; ++addr) {
            uint32_t actual = ramReadA(addr);
            uint32_t expected = ref.read(addr);
            result.total_checks++;

            if (actual != expected) {
                reportError(result, 6, addr, expected, actual, "Port A read mismatch after B write");
            }

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  A-Read progress: %d/%d (errors=%d)\r", addr + 1, depth_, result.error_count);
                fflush(stdout);
            }
        }
        printf("  A-Read progress: %d/%d (errors=%d)  [%s]\n",
               depth_, depth_, result.error_count, result.passed() ? "PASS" : "FAIL");
    } else {
        // Asymmetric dual-port: A-port self-test
        ReferenceMemory ref(depth_, width_);

        printf("\n[TEST 6] Asymmetric dual-port: Port A self-test (depth=%d, width=%d)\n", depth_, width_);
        printf("[TEST 6] Port A reverse write all addresses...\n");

        for (int addr = depth_ - 1; addr >= 0; --addr) {
            uint32_t data = testPatternInv(addr, width_);
            ramWriteA(addr, data);
            ref.write(addr, data);

            if (print_interval_ > 0 && (depth_ - addr) % print_interval_ == 0) {
                printf("  A-Write progress: %d/%d\r", depth_ - addr, depth_);
                fflush(stdout);
            }
        }
        printf("  A-Write progress: %d/%d  [OK]\n", depth_, depth_);

        uint64_t idle = buildWriteDataAB(0, 0, 0, 0, 0, 0, 0, 0);
        usb_.writeRead(idle);
        usb_.writeRead(idle);

        printf("[TEST 6] Port A sequential read all addresses...\n");

        for (int addr = 0; addr < depth_; ++addr) {
            uint32_t actual = ramReadA(addr);
            uint32_t expected = ref.read(addr);
            result.total_checks++;

            if (actual != expected) {
                reportError(result, 6, addr, expected, actual, "Port A self-test read mismatch");
            }

            if (print_interval_ > 0 && (addr + 1) % print_interval_ == 0) {
                printf("  A-Read progress: %d/%d (errors=%d)\r", addr + 1, depth_, result.error_count);
                fflush(stdout);
            }
        }
        printf("  A-Read progress: %d/%d (errors=%d)  [%s]\n",
               depth_, depth_, result.error_count, result.passed() ? "PASS" : "FAIL");
    }

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

    if (is_dual_port_) {
        results.push_back(test5PortAWritePortBRead());
        results.push_back(test6PortBWritePortARead());
    }

    return results;
}

} // namespace hw_test
