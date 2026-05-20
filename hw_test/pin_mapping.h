#pragma once
#ifndef PIN_MAPPING_H
#define PIN_MAPPING_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace hw_test {

enum class PinType {
    Input,
    Output,
    Clock,
    Unknown
};

struct SignalBit {
    std::string signal_name;  // e.g. "addr_a[0]"
    std::string base_name;    // e.g. "addr_a"
    int bit_index;            // bit index, e.g. 0
    std::string pin_name;     // e.g. "P151"
    PinType pin_type;         // Input / Output / Clock
    int bit_position;         // 64-bit position (0~63)
};

class PinMapping {
public:
    PinMapping();
    ~PinMapping() = default;

    bool loadFromFile(const std::string& xml_path);
    int getBitPosition(const std::string& signal_name) const;
    PinType getPinType(const std::string& signal_name) const;
    const std::vector<SignalBit>& getAllBits() const { return bits_; }
    std::vector<const SignalBit*> getSignalBits(const std::string& base_name) const;
    void printMapping() const;

private:
    std::vector<SignalBit> bits_;
    std::map<std::string, size_t> signal_to_index_;

    static int inputPinToBitIndex(const std::string& pin_name);
    static int outputPinToBitIndex(const std::string& pin_name);
};

} // namespace hw_test

#endif // PIN_MAPPING_H
