#version 460 core

in vec3 v_world_pos;
in vec3 v_world_normal;
flat in uint v_matid;

layout(location = 0) out vec4 o_pos_matid;
layout(location = 1) out vec4 o_normal;

void main() {
    // Mirror the tracer's `set_face_normal`: the "surface" normal always points toward
    // the viewer, regardless of winding. This lets scenes use either winding convention —
    // the Cornell box's box-lid tri-quads wind such that cross(u,v) points away from the
    // camera, which would otherwise cull with backface culling + the raw vertex normal.
    vec3 n = normalize(v_world_normal);
    if (!gl_FrontFacing) {
        n = -n;
    }

    o_pos_matid = vec4(v_world_pos, uintBitsToFloat(v_matid));
    o_normal = vec4(n, 0.0);
}
