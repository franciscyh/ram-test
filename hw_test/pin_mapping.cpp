#include "pin_mapping.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace hw_test {

// Extracted from rabbit_App/src/Ports/PinInfo.cpp
int PinMapping::inputPinToBitIndex(const std::string& pin) {
    static const std::map<std::string, int> map = {
        {"P151", 0},  {"P148", 1},  {"P150", 2},  {"P152", 3},  {"P160", 4},
        {"P161", 5},  {"P162", 6},  {"P163", 7},  {"P164", 8},  {"P165", 9},
        {"P166", 10}, {"P169", 11}, {"P173", 12}, {"P174", 13}, {"P175", 14},
        {"P191", 15}, {"P120", 16}, {"P116", 17}, {"P115", 18}, {"P114", 19},
        {"P113", 20}, {"P112", 21}, {"P111", 22}, {"P108", 23}, {"P102", 24},
        {"P101", 25}, {"P100", 26}, {"P97", 27},  {"P96", 28},  {"P95", 29},
        {"P89", 30},  {"P88", 31},  {"P87", 32},  {"P86", 33},  {"P81", 34},
        {"P75", 35},  {"P74", 36},  {"P70", 37},  {"P69", 38},  {"P68", 39},
        {"P64", 40},  {"P62", 41},  {"P61", 42},  {"P58", 43},  {"P57", 44},
        {"P49", 45},  {"P47", 46},  {"P48", 47},  {"P192", 48}, {"P193", 49},
        {"P199", 50}, {"P200", 51}, {"P201", 52}, {"P202", 53}
    };
    auto it = map.find(pin);
    return (it != map.end()) ? it->second : -1;
}

int PinMapping::outputPinToBitIndex(const std::string& pin) {
    static const std::map<std::string, int> map = {
        {"P7", 1},    {"P6", 2},    {"P5", 3},    {"P4", 4},    {"P9", 5},
        {"P8", 6},    {"P16", 7},   {"P15", 8},   {"P11", 9},   {"P10", 10},
        {"P20", 11},  {"P18", 12},  {"P17", 13},  {"P22", 14},  {"P21", 15},
        {"P23", 16},  {"P44", 17},  {"P45", 18},  {"P46", 19},  {"P43", 20},
        {"P40", 21},  {"P41", 22},  {"P42", 23},  {"P33", 24},  {"P34", 25},
        {"P35", 26},  {"P36", 27},  {"P30", 28},  {"P31", 29},  {"P24", 30},
        {"P27", 31},  {"P29", 32},  {"P110", 33}, {"P109", 34}, {"P99", 35},
        {"P98", 36},  {"P94", 37},  {"P93", 38},  {"P84", 39},  {"P83", 40},
        {"P82", 41},  {"P73", 42},  {"P71", 43},  {"P63", 44},  {"P60", 45},
        {"P59", 46},  {"P56", 47},  {"P55", 48},  {"P167", 49}, {"P168", 50},
        {"P176", 51}, {"P187", 52}, {"P189", 53}, {"P194", 54}
    };
    auto it = map.find(pin);
    if (it != map.end()) {
        // output_decl_index_map is 1-based, but read_data uses 0-based positions
        return it->second - 1;
    }
    return -1;
}

PinMapping::PinMapping() {}

// Helper: extract attribute value from XML tag line
// e.g. line='<port name="addr_a[0]" position="P151" />'
// findAttr(line, "name=") returns "addr_a[0]"
static std::string findAttr(const std::string& line, const char* key) {
    size_t pos = line.find(key);
    if (pos == std::string::npos) return "";
    pos += strlen(key);
    if (pos >= line.size() || line[pos] != '"') return "";
    pos++;
    size_t end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

bool PinMapping::loadFromFile(const std::string& xml_path) {
    std::ifstream file(xml_path);
    if (!file.is_open()) {
        fprintf(stderr, "[ERROR] Cannot open constraint file: %s\n", xml_path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for <port name="..." position="..." /> tags
        if (line.find("<port") == std::string::npos) continue;
        if (line.find("name=") == std::string::npos) continue;
        if (line.find("position=") == std::string::npos) continue;

        SignalBit bit;
        bit.signal_name = findAttr(line, "name=");
        bit.pin_name = findAttr(line, "position=");
        if (bit.signal_name.empty() || bit.pin_name.empty()) continue;

        // Parse base_name and bit_index from signal_name like "addr_a[0]"
        size_t bracket_pos = bit.signal_name.find('[');
        if (bracket_pos != std::string::npos) {
            bit.base_name = bit.signal_name.substr(0, bracket_pos);
            size_t close_pos = bit.signal_name.find(']', bracket_pos);
            if (close_pos != std::string::npos) {
                bit.bit_index = std::stoi(bit.signal_name.substr(bracket_pos + 1, close_pos - bracket_pos - 1));
            }
        } else {
            bit.base_name = bit.signal_name;
            bit.bit_index = 0;
        }

        // Determine pin type and 64-bit position
        if (bit.pin_name == "P77") {
            bit.pin_type = PinType::Clock;
            bit.bit_position = -1; // Clock not controlled via 64-bit data
        } else {
            int idx = inputPinToBitIndex(bit.pin_name);
            if (idx >= 0) {
                bit.pin_type = PinType::Input;
                bit.bit_position = idx;
            } else {
                idx = outputPinToBitIndex(bit.pin_name);
                if (idx >= 0) {
                    bit.pin_type = PinType::Output;
                    bit.bit_position = idx;
                } else {
                    bit.pin_type = PinType::Unknown;
                    bit.bit_position = -1;
                    fprintf(stderr, "[WARN] Unknown pin: %s for signal %s\n",
                            bit.pin_name.c_str(), bit.signal_name.c_str());
                }
            }
        }

        bits_.push_back(bit);
        signal_to_index_[bit.signal_name] = bits_.size() - 1;
    }

    return !bits_.empty();
}

int PinMapping::getBitPosition(const std::string& signal_name) const {
    auto it = signal_to_index_.find(signal_name);
    if (it != signal_to_index_.end()) {
        return bits_[it->second].bit_position;
    }
    return -1;
}

PinType PinMapping::getPinType(const std::string& signal_name) const {
    auto it = signal_to_index_.find(signal_name);
    if (it != signal_to_index_.end()) {
        return bits_[it->second].pin_type;
    }
    return PinType::Unknown;
}

std::vector<const SignalBit*> PinMapping::getSignalBits(const std::string& base_name) const {
    std::vector<const SignalBit*> result;
    for (const auto& bit : bits_) {
        if (bit.base_name == base_name && bit.pin_type != PinType::Clock) {
            result.push_back(&bit);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const SignalBit* a, const SignalBit* b) {
                  return a->bit_index < b->bit_index;
              });
    return result;
}

void PinMapping::printMapping() const {
    printf("=== Pin Mapping Table ===\n");
    printf("%-20s %-10s %-10s %-10s %-10s\n",
           "Signal", "Pin", "Type", "BitIdx", "64bitPos");
    printf("------------------------------------------------------------------\n");
    for (const auto& bit : bits_) {
        const char* type_str = "Unknown";
        switch (bit.pin_type) {
            case PinType::Input:  type_str = "Input"; break;
            case PinType::Output: type_str = "Output"; break;
            case PinType::Clock:  type_str = "Clock"; break;
            default: break;
        }
        printf("%-20s %-10s %-10s %-10d %-10d\n",
               bit.signal_name.c_str(), bit.pin_name.c_str(),
               type_str, bit.bit_index, bit.bit_position);
    }
    printf("------------------------------------------------------------------\n");
    printf("Total signals: %zu\n", bits_.size());
}

} // namespace hw_test
