

#define DECLARE_QUEUE(NAME, BIND_COUNT, BIND_IDX)                                        \
      layout(std430, binding = BIND_COUNT) restrict buffer NAME##_Counter { uint NAME##_count; }; \
layout(std430, binding = BIND_IDX) restrict buffer NAME##_Indices { uint NAME##_idx[]; }; \
void NAME##_push(uint pid) { uint s = atomicAdd(NAME##_count, 1u); NAME##_idx[s] = pid; }

DECLARE_QUEUE(ray_queue, 11, 12);
DECLARE_QUEUE(hit_lambertian, 13, 14);
DECLARE_QUEUE(hit_metal, 15, 16);
DECLARE_QUEUE(hit_dielectric, 17, 18);
DECLARE_QUEUE(hit_emissive, 19, 20);
DECLARE_QUEUE(shadow_queue, 21, 22);
