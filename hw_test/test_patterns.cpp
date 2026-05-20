#include "test_patterns.h"

namespace hw_test {

uint32_t testPattern(int addr, int width) {
    uint32_t result = 0;
    // Corresponds to Verilog: {addr[3:0], addr[7:4]^addr[3:0], addr[11:8]^addr[7:4], addr[15:12]^addr[11:8]}
    uint32_t a0 = addr & 0xF;           // addr[3:0]
    uint32_t a1 = (addr >> 4) & 0xF;    // addr[7:4]
    uint32_t a2 = (addr >> 8) & 0xF;    // addr[11:8]
    uint32_t a3 = (addr >> 12) & 0xF;   // addr[15:12]

    result |= a0;
    result |= (a1 ^ a0) << 4;
    result |= (a2 ^ a1) << 8;
    result |= (a3 ^ a2) << 12;

    return result & widthMask(width);
}

uint32_t testPatternInv(int addr, int width) {
    return (~testPattern(addr, width)) & widthMask(width);
}

uint32_t testPatternStride(int addr, int stride, int width) {
    return (testPattern(addr, width) ^ (stride & widthMask(width))) & widthMask(width);
}

uint32_t widthMask(int width) {
    if (width >= 32) return 0xFFFFFFFFu;
    return (1u << width) - 1;
}

ReferenceMemory::ReferenceMemory(int depth, int width)
    : depth_(depth), width_(width), mask_(widthMask(width)), mem_(depth, 0) {}

void ReferenceMemory::write(int addr, uint32_t data) {
    if (addr >= 0 && addr < depth_) {
        mem_[addr] = data & mask_;
    }
}

uint32_t ReferenceMemory::read(int addr) const {
    if (addr >= 0 && addr < depth_) {
        return mem_[addr] & mask_;
    }
    return 0;
}

void ReferenceMemory::initWithPattern() {
    for (int i = 0; i < depth_; ++i) {
        mem_[i] = testPattern(i, width_);
    }
}

void ReferenceMemory::initWithPatternInv() {
    for (int i = 0; i < depth_; ++i) {
        mem_[i] = testPatternInv(i, width_);
    }
}

void ReferenceMemory::initWithPatternStride(int stride) {
    for (int i = 0; i < depth_; ++i) {
        mem_[i] = testPatternStride(i, stride, width_);
    }
}

} // namespace hw_test
