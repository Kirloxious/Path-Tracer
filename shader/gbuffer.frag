#version 460 core

in vec3 v_world_pos;
in vec3 v_world_normal;
flat in uint v_matid;
in vec4 v_curr_clip;
in vec4 v_prev_clip;

layout(location = 0) out vec4 o_pos_matid;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec2 o_motion;

void main() {
    o_pos_matid = vec4(v_world_pos, uintBitsToFloat(v_matid));
    o_normal = vec4(normalize(v_world_normal), 0.0);

    // NDC-space motion vector: (curr - prev) in [-1, 1] x/y range.
    // Unused in phase 1; written so phase 3's temporal reprojection can sample it directly.
    vec2 curr_ndc = v_curr_clip.xy / v_curr_clip.w;
    vec2 prev_ndc = v_prev_clip.xy / v_prev_clip.w;
    o_motion = (curr_ndc - prev_ndc) * 0.5;
}
