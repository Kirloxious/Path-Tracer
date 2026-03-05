#pragma once

#include <vector>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>

#include "primitive.h"
#include "bvh.h"

// UNUSED 

struct MortonPrimitive{
    uint32_t code;
    int index;

    uint32_t expandBits(uint32_t v) {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    }
    
    uint32_t morton3D(float x, float y, float z) {
        x = glm::clamp(x * 1024.0f, 0.0f, 1023.0f);
        y = glm::clamp(y * 1024.0f, 0.0f, 1023.0f);
        z = glm::clamp(z * 1024.0f, 0.0f, 1023.0f);
    
        return (expandBits((uint32_t)x) << 2) |
                (expandBits((uint32_t)y) << 1) |
                expandBits((uint32_t)z);
    }
    
    
    int findSplit(const std::vector<MortonPrimitive>& mortonPrims, int first, int last) {
        uint32_t firstCode = mortonPrims[first].code;
        uint32_t lastCode  = mortonPrims[last - 1].code; 
    
        if (firstCode == lastCode) {
            return (first + last) >> 1;
        }
    
        int commonPrefix = __builtin_clz(firstCode ^ lastCode);
    
        int split = first;
        int step = last - first;
    
        while (step > 1) {
            int mid = split + (step >> 1);
            uint32_t midCode = mortonPrims[mid].code;
            int midPrefix = __builtin_clz(firstCode ^ midCode);
    
            if (midPrefix > commonPrefix)
                split = mid;
    
            step >>= 1;
        }
    
        return split;
    }
};



