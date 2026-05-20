#pragma once
#ifndef TEST_PATTERNS_H
#define TEST_PATTERNS_H

#include <cstdint>
#include <vector>

namespace hw_test {

// Test data generators (C++ version of tb_test.sv test_pattern)

uint32_t testPattern(int addr, int width);
uint32_t testPatternInv(int addr, int width);
uint32_t testPatternStride(int addr, int stride, int width);
uint32_t widthMask(int width);

// Reference memory model: tracks expected RAM state
class ReferenceMemory {
public:
    ReferenceMemory(int depth, int width);

    void write(int addr, uint32_t data);
    uint32_t read(int addr) const;

    int depth() const { return depth_; }
    int width() const { return width_; }

    void initWithPattern();
    void initWithPatternInv();
    void initWithPatternStride(int stride);

private:
    int depth_;
    int width_;
    uint32_t mask_;
    std::vector<uint32_t> mem_;
};

} // namespace hw_test

#endif // TEST_PATTERNS_H
