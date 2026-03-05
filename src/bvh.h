#pragma once

#include <vector>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>

#include "primitive.h"

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    
    inline glm::vec3 center() const {
        return (min + max) * 0.5f;
    }
    
    inline AABB& expand(float delta)  {
        float padding = delta/2;
        this->min = glm::vec3(min.x - padding, min.y - padding, min.z - padding);
        this->max = glm::vec3(max.x + padding, max.y + padding, max.z + padding);  
        return *this;
    }
        
    inline float surfaceArea() const {
        glm::vec3 d = max - min;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
        
    inline AABB& pad() {
        float delta = 0.001f;
        if(this->surfaceArea() < delta){
            this->expand(delta);
        }
        return *this;
    }
};
    
AABB surroundingBox(const AABB& a, const AABB& b);
AABB computeAABB(const Sphere& s);
AABB computeAABB(const Quad& q);


struct alignas(16) BVHNodeFlat {
    glm::vec4  aabbMin;  // .xyz = min
    glm::vec4  aabbMax;  // .xyz = max
    glm::ivec4 meta;     // .x = left, .y = right, .z = primitiveIndex, .w = nextAfterSubtree
};

class BVH {
public:
    std::vector<BVHNodeFlat> nodes;
    int root = -1;
    
    void build(const std::vector<Sphere>& spheres);

private:
    struct Node {
        AABB aabb;
        int left = -1;
        int right = -1;
        int primitiveIndex = -1;

        bool isLeaf() const {return primitiveIndex != -1;}
    };

    static float computeSAHCost(int numLeft, float leftArea, int numRight, float rightArea);

    static void findBestSplit(const std::vector<AABB>& aabbs, const std::vector<int>& indices, int& splitIndex, 
                              std::vector<int>& outSorted, std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight);

    static int buildR(std::vector<Node>& bvh, const std::vector<AABB>& aabbs, std::vector<int> indices, 
                      std::vector<AABB>& scratchLeft, std::vector<AABB>& scratchRight);

    int flatten(int nodeIndex, const std::vector<Node>& nodes, int nextAfterSubtree);


};









