#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

#include "pin_mapping.h"
#include "usb_driver.h"
#include "ram_test_cases.h"

namespace fs = std::filesystem;
using namespace hw_test;

// ============================================================================
// Command line help
// ============================================================================

void printUsage(const char* prog) {
    printf("Usage: %s [options] <config_dir>\n", prog);
    printf("\n");
    printf("  <config_dir>  Test configuration directory (e.g. test/4x2048)\n");
    printf("\n");
    printf("Options:\n");
    printf("  -f <freq>     Set test frequency in Hz (default: 200)\n");
    printf("  -b <bitfile>  Specify bitstream file (default: <config_dir>/top_yosys_bit.bit)\n");
    printf("  -t <test>     Run specific test only (1-4, default: all)\n");
    printf("  -i <interval> Print progress every N addresses (default: 100, 0=disable)\n");
    printf("  -v            Verbose: print pin mapping\n");
    printf("  -h            Show this help\n");
    printf("\n");
    printf("Tests:\n");
    printf("  1: Sequential write/read all addresses\n");
    printf("  2: Reverse order write/read\n");
    printf("  3: Stride access\n");
    printf("  4: Random access\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test/4x2048\n", prog);
    printf("  %s -f 100 -t 1 test/4x2048\n", prog);
    printf("  %s -b mydesign.bit test/4x2048\n", prog);
}

// ============================================================================
// Print final report
// ============================================================================

void printReport(const std::vector<TestResult>& results) {
    printf("\n");
    printf("============================================================\n");
    printf("                    HARDWARE TEST REPORT                     \n");
    printf("============================================================\n");
    printf("%-30s %10s %10s %12s %s\n", "Test", "Checks", "Errors", "Time(ms)", "Result");
    printf("------------------------------------------------------------\n");

    int total_checks = 0;
    int total_errors = 0;

    for (const auto& r : results) {
        printf("%-30s %10d %10d %12.1f %s\n",
               r.test_name.c_str(),
               r.total_checks,
               r.error_count,
               r.elapsed_ms,
               r.passed() ? "PASS" : "FAIL");
        total_checks += r.total_checks;
        total_errors += r.error_count;
    }

    printf("------------------------------------------------------------\n");
    printf("%-30s %10d %10d\n", "TOTAL", total_checks, total_errors);
    printf("============================================================\n");
    printf("RESULT: %s\n", total_errors == 0 ? "ALL PASSED" : "SOME FAILED");
    printf("============================================================\n");

    for (const auto& r : results) {
        if (!r.errors.empty()) {
            printf("\n[%s] First %zu errors:\n", r.test_name.c_str(),
                   std::min(r.errors.size(), size_t(10)));
            for (size_t i = 0; i < std::min(r.errors.size(), size_t(10)); ++i) {
                const auto& e = r.errors[i];
                printf("  Addr=0x%04X (=%d) Expected=0x%X Actual=0x%X (%s)\n",
                       e.addr, e.addr, e.expected, e.actual, e.desc.c_str());
            }
            if (r.errors.size() > 10) {
                printf("  ... and %zu more errors\n", r.errors.size() - 10);
            }
        }
    }
}

// ============================================================================
// Infer width/depth from config directory name
// ============================================================================

bool inferConfig(const fs::path& config_dir, int& width, int& depth) {
    std::string name = config_dir.filename().string();

    // Try parsing "4x2048" format
    size_t x_pos = name.find('x');
    if (x_pos != std::string::npos) {
        try {
            width = std::stoi(name.substr(0, x_pos));
            depth = std::stoi(name.substr(x_pos + 1));
            printf("[INFO] Inferred config from dir name: width=%d, depth=%d\n", width, depth);
            return true;
        } catch (...) {
            // parsing failed, try next method
        }
    }

    // Try parsing from test.v
    fs::path test_v = config_dir / "test.v";
    if (fs::exists(test_v)) {
        FILE* f = fopen(test_v.string().c_str(), "r");
        if (f) {
            char line[256];
            int addr_width = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "DIA") || strstr(line, "DI")) {
                    int msb = 0, lsb = 0;
                    if (sscanf(line, " input [%d:%d]", &msb, &lsb) == 2) {
                        width = msb - lsb + 1;
                    }
                }
                if (strstr(line, "ADDRA")) {
                    int msb = 0, lsb = 0;
                    if (sscanf(line, " input [%d:%d]", &msb, &lsb) == 2) {
                        addr_width = msb - lsb + 1;
                        depth = 1 << addr_width;
                    }
                }
            }
            fclose(f);
            if (width > 0 && depth > 0) {
                printf("[INFO] Inferred config from test.v: width=%d, depth=%d\n", width, depth);
                return true;
            }
        }
    }

    fprintf(stderr, "[ERROR] Cannot infer width/depth from config dir: %s\n", name.c_str());
    fprintf(stderr, "        Please use directory name like '4x2048' or specify manually.\n");
    return false;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    std::string config_dir;
    std::string bitfile_path;
    int frequency = 200;
    int specific_test = 0; // 0 = all
    int print_interval = 100;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            frequency = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            bitfile_path = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            specific_test = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            print_interval = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (argv[i][0] != '-') {
            config_dir = argv[i];
        } else {
            fprintf(stderr, "[ERROR] Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    if (config_dir.empty()) {
        fprintf(stderr, "[ERROR] Missing config directory\n");
        printUsage(argv[0]);
        return 1;
    }

    fs::path config_path(config_dir);
    if (!fs::exists(config_path)) {
        fprintf(stderr, "[ERROR] Config directory not found: %s\n", config_dir.c_str());
        return 1;
    }

    int width = 0, depth = 0;
    if (!inferConfig(config_path, width, depth)) {
        return 1;
    }

    if (bitfile_path.empty()) {
        fs::path default_bit = config_path / "top_yosys_bit.bit";
        bitfile_path = default_bit.string();
    }

    if (!fs::exists(bitfile_path)) {
        fprintf(stderr, "[ERROR] Bitstream not found: %s\n", bitfile_path.c_str());
        fprintf(stderr, "        Run 'run_flow.bat %s' first to generate bitstream.\n", config_dir.c_str());
        return 1;
    }

    fs::path cons_path = config_path / "top_cons.xml";
    if (!fs::exists(cons_path)) {
        fprintf(stderr, "[ERROR] Constraint file not found: %s\n", cons_path.string().c_str());
        return 1;
    }

    printf("============================================================\n");
    printf("           RAM Hardware Test (FDP3P7 via USB/SMIMS)         \n");
    printf("============================================================\n");
    printf("Config dir:  %s\n", config_dir.c_str());
    printf("Bitstream:   %s\n", bitfile_path.c_str());
    printf("Constraint:  %s\n", cons_path.string().c_str());
    printf("Width:       %d bits\n", width);
    printf("Depth:       %d words\n", depth);
    printf("Frequency:   %d Hz\n", frequency);
    printf("Print interval: %d\n", print_interval);
    printf("============================================================\n");

    PinMapping pin_map;
    if (!pin_map.loadFromFile(cons_path.string())) {
        fprintf(stderr, "[ERROR] Failed to load pin mapping\n");
        return 1;
    }

    if (verbose) {
        pin_map.printMapping();
    } else {
        printf("[OK] Loaded %zu pin mappings\n", pin_map.getAllBits().size());
    }

    UsbDriver usb;
    usb.setCyclePeriodMs(1000 / frequency);

    printf("\n[1/4] Programming FPGA...\n");
    if (!usb.programFpga(bitfile_path)) {
        return 1;
    }

    printf("[2/4] Opening USB device...\n");
    if (!usb.open()) {
        return 1;
    }

    printf("[3/4] Warming up...\n");
    for (int i = 0; i < 4; ++i) {
        usb.writeRead(0);
    }

    printf("[4/4] Running tests...\n");
    RamTestExecutor executor(pin_map, usb, width, depth);
    executor.setFrequency(frequency);
    executor.setPrintInterval(print_interval);

    std::vector<TestResult> results;

    switch (specific_test) {
        case 1: results.push_back(executor.test1SequentialWriteRead()); break;
        case 2: results.push_back(executor.test2ReverseWriteRead()); break;
        case 3: results.push_back(executor.test3StrideAccess()); break;
        case 4: results.push_back(executor.test4RandomAccess()); break;
        default: results = executor.runAllTests(); break;
    }

    usb.close();

    printReport(results);

    for (const auto& r : results) {
        if (!r.passed()) return 1;
    }
    return 0;
}
