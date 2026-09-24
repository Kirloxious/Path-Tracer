#ifndef MATH_GLSL
#define MATH_GLSL

const float PI = 3.14159265358979323846;

// Rec. 709 luminance of linear RGB.
float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

#endif
