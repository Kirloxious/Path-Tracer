#version 460 core

#define MAX_NUM_SPHERES 10

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

/* Uniforms */

layout(rgba32f, location = 0)  readonly uniform image2D srcImage;
layout(rgba32f, location = 1) writeonly uniform image2D destImage;
layout(location = 3) uniform int time;
layout(location = 4) uniform int frameIndex;
layout(location = 5) uniform vec2 imageDimensions;
layout(location = 6) uniform int num_objects;
layout(location = 7) uniform int bvh_size;
layout(location = 8) uniform int root_index;
layout(location = 9) uniform int samples_per_pixel;
layout(location = 10) uniform int max_bounces;

/* Structs */

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct HitRecord {
    vec3 point;
    vec3 normal;
    uint mat_index;
    float t;
    bool front_face;
};

struct Sphere{
    vec3 position;
    float radius;
    uint material_index;
};

struct Quad{
    vec3 corner_point;
    vec3 u;
    vec3 v;
    vec3 w;
    uint material_index;
};

struct Material{
    vec3 color;
    float fuzz;
    vec3 emission;
    float refractive_index;
    float type;
};

struct BVHNodeFlat {
    vec4 aabbMin;
    vec4 aabbMax;
    ivec4 meta; // x: left, y: right, z: sphereIndex, w: next node
};


/* Scnene Buffers */

layout(std430, binding = 0) buffer SpheresBuffer{
    Sphere spheres[];
};

layout(std430, binding = 1) buffer MatsBuffer{
    Material mats[];
};

layout(std140, binding = 2) uniform Camera {
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 invViewMatrix;
    mat4 invProjMatrix;
    vec3 cameraPosition;
    float focus_distance;
    float defocus_angle;
};

layout(std430, binding = 3) buffer BVHBuffer {
    BVHNodeFlat nodes[];
};

/* Constants */

const float MAT_LAMBERTIAN = 0.0;
const float MAT_METAL = 1.0;
const float MAT_DIELECTRIC = 2.0;
const float MAT_EMISSIVE = 3.0;

const float infinity = 1./0.;
const float PI = 3.1415926535897932384626433832795;


/* Helper Math Functions */

// The state must be initialized to non-zero value
uint XOrShift32(inout uint state)
{
    // https://en.wikipedia.org/wiki/Xorshift
    uint x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

float RandomUnilateral(inout uint state) {
    return float(XOrShift32(state)) / float(uint(4294967295));
}

float RandomBilateral(inout uint state) {
    return 2.0f * RandomUnilateral(state) - 1.0f;
}

vec3 random_vec3(uint state) {
    return vec3(RandomBilateral(state), RandomBilateral(state), RandomBilateral(state));
}


vec3 random_unit_vector(uint state) {
    vec3 p;
    float lensq;
    do {
        p = random_vec3(state);
        lensq = dot(p, p);
    } while (lensq <= 1e-160 || lensq > 1.0);
    
    return p / sqrt(lensq);
}

vec3 random_on_hemisphere(uint state, vec3 normal) {
    vec3 direction = random_unit_vector(state);
    return (dot(direction, normal) > 0.0) ? direction : -direction;
}


/** Intersection functions **/

bool set_face_normal(in Ray ray, in vec3 outward_normal, inout HitRecord hit_rec) {
    hit_rec.front_face = dot(ray.direction, outward_normal) < 0.0;
    hit_rec.normal = hit_rec.front_face ? outward_normal : -outward_normal;
    return true;
}

bool hit_sphere(in Ray r, in Sphere s, float ray_tmin, float ray_tmax, inout HitRecord hit_rec) {
    vec3 oc = s.position - r.origin; 
    float a = dot(r.direction, r.direction);
    float h = dot(oc, r.direction);
    float c = dot(oc, oc) - s.radius * s.radius; 
    
    float discriminant = h * h - a * c;
    if (discriminant < 0.0) 
        return false;

    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a;
    if (root < ray_tmin || root > ray_tmax){
        root = (h + sqrtd) / a;
        if (root < ray_tmin || root > ray_tmax)
            return false;
    }
    
    hit_rec.t = root;
    hit_rec.point = r.origin + hit_rec.t * r.direction ;
    hit_rec.normal = normalize(hit_rec.point - s.position);
    hit_rec.mat_index = s.material_index;
    set_face_normal(r, hit_rec.normal, hit_rec);

    return true;
}

bool hit_quad(in Ray r, in Quad q, inout HitRecord hit_rec) {
    // constants of the quad for intersection calculations
    vec3 u = q.u;
    vec3 v = q.v;
    vec3 n = cross(u, v);
    vec3 normal = normalize(n);
    float D = dot(normal, q.corner_point);
    vec3 w = n / dot(n, n);

    float denom = dot(normal, r.direction);

    // Ray is parallel to quad
    if (denom < 0.00001){
        return false;
    }

    float t = (D - dot(normal, r.origin)) / denom;
    if (t < 0.00001){
        return false;
    }

    // determine if hit point lies within planar shape
    vec3 intersection = r.origin + hit_rec.t * r.direction; 
    vec3 planar_hitpt_vector = intersection - q.corner_point;
    float alpha = dot(w, cross(planar_hitpt_vector, v));
    float beta = dot(w, cross(u, planar_hitpt_vector));

    // check if hit point is outside planar 
    if ((alpha) < 0.0 || (alpha) > 1.0 || (beta) < 0.0 || (beta) > 1.0){
        return false;
    }

    hit_rec.t = t;
    hit_rec.point = intersection;
    hit_rec.normal = normal;
    hit_rec.mat_index = q.material_index;
    set_face_normal(r, hit_rec.normal, hit_rec);

    return true;
}

/** End Intersections **/


/** Ray functions **/

// Brute force ray-sphere intersection
bool world_hit(in Ray r, in float ray_tmin,  in float ray_tmax, inout HitRecord hit_rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_so_far = ray_tmax;
    for (int i = 0; i < num_objects; i++) {
        Sphere s = spheres[i];
        if (hit_sphere(r, s, ray_tmin, closest_so_far, temp_rec)) {
            if (temp_rec.t < closest_so_far){
                hit_anything = true;
                closest_so_far = temp_rec.t;
                hit_rec = temp_rec;
            }
        }
    }
    return hit_anything;
}

bool intersect_aabb(in Ray r, in vec3 min_b, in vec3 max_b, in vec3 inv_dir) {
    vec3 tmin = (min_b - r.origin) * inv_dir;
    vec3 tmax = (max_b - r.origin) * inv_dir;
    
    vec3 t1 = min(tmin, tmax);
    vec3 t2 = max(tmin, tmax);
    
    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far = min(min(t2.x, t2.y), t2.z);
    
    return t_near < t_far && t_far > 0.0f;
}


// BVH traversal intersection using pointers
bool world_hit_aabb_stackless(in Ray r, in float tMin, in float tMax, inout HitRecord hit) {
    vec3 invDir = 1.0 / r.direction;
    int idx = root_index;
    bool hitSomething = false;
    float closest = tMax;
    
    while(idx >= 0) {
        BVHNodeFlat node = nodes[idx];
        if (intersect_aabb(r, node.aabbMin.xyz, node.aabbMax.xyz, invDir)) {
            // Check if leaf
            if (node.meta.z != -1) {
                Sphere s = spheres[node.meta.z];
                HitRecord temp;
                if (hit_sphere(r, s, tMin, closest, temp)) {
                    closest = temp.t;
                    hit = temp;
                    hitSomething = true;
                }
                idx = node.meta.w; // move to next node using the next pointer
            }
            else {
                // Not a leaf: move to left child
                idx = node.meta.x; 
            }
        } else {
            idx = node.meta.w; // skip to next node if AABB missed
        }
    }
    return hitSomething;
}


float reflectance(float cosine, float ref_idx) {
    // Schlick's approximation
    cosine = clamp(cosine, 0.0, 1.0);
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

bool scatter(uint state, in Ray ray_in, in HitRecord hit_rec, inout vec3 matColor, inout Ray scattered) {
    Material mat = mats[hit_rec.mat_index];
    float type = mat.type;

    if (type == MAT_EMISSIVE) {
        // Emissive materials don't scatter
        matColor = mat.color;
        return false;
    }

    if (type == MAT_LAMBERTIAN) {
        vec3 scatter_dir = hit_rec.normal + random_unit_vector(state);

        if (length(scatter_dir) < 0.0001)
            scatter_dir = hit_rec.normal;

        scattered = Ray(hit_rec.point, normalize(scatter_dir));
        matColor = mat.color;
        return true;
    }

    if (type == MAT_METAL) {
        vec3 reflected = reflect(normalize(ray_in.direction), hit_rec.normal);
        reflected += mat.fuzz * random_unit_vector(state); // .a = fuzz
        scattered = Ray(hit_rec.point, normalize(reflected));
        matColor = mat.color;
        return (dot(scattered.direction, hit_rec.normal) > 0.0);
    }

    if (type == MAT_DIELECTRIC) {
        matColor = vec3(1.0);
        float ri = hit_rec.front_face ? (1.0 / mat.refractive_index) : mat.refractive_index;

        vec3 unit_dir = normalize(ray_in.direction);
        float cos_theta = clamp(dot(-unit_dir, hit_rec.normal), 0.0, 1.0);
        float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;
        if (cannot_refract || reflectance(cos_theta, ri) > RandomUnilateral(state))
            direction = reflect(unit_dir, hit_rec.normal);
        else
            direction = refract(unit_dir, hit_rec.normal, ri);

        // Catch degenerate rays
        if(length(direction) < 0.0001) 
            direction = hit_rec.normal; 

        scattered = Ray(hit_rec.point, normalize(direction));
        return true;
    }

    // Default: absorb
    return false;
}

bool ray_contributes_to_color(vec3 color, float threshold) {
    return length(color) > 0.001;  // Only continue if the color is above the threshold
}

// Original ray color
vec3 ray_color(in Ray ray, uint max_bounces, inout uint state) {
    vec3 accumulated_color = vec3(1.0);
    vec3 radiance = vec3(0.0);
    
    Ray current_ray;
    current_ray.origin = ray.origin;
    current_ray.direction = ray.direction;

    for (int bounce = 0; bounce < max_bounces; bounce++) {
        HitRecord hit_rec;
        if (world_hit(current_ray, 0.001, infinity, hit_rec)) {
            Ray scattered;
            vec3 matColor;
            vec3 emitted = mats[hit_rec.mat_index].emission;

            if (scatter(state, current_ray, hit_rec, matColor, scattered)) {
                accumulated_color *= matColor;
                current_ray = scattered;

                // end termination if contribution is below a threshold
                if (!ray_contributes_to_color(accumulated_color, 0.0001))
                    break;  // Stop further bounces
            } 
            else { // no scatter
                radiance += accumulated_color * matColor * emitted;
                break;
            }
        } else { // no hit
            vec3 unit_direction = normalize(current_ray.direction);
            float blend = 0.5 * (unit_direction.y + 1.0);
            vec3 background_color = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), blend);
            // vec3 background_color = vec3(0.05);

            radiance += accumulated_color * background_color;
            break;
        }
    }
    return radiance; // max depth reached
}



vec3 ray_color2(in Ray ray, uint max_bounces, inout uint state) {
    vec3 accumulated_color = vec3(1.0);
    vec3 final_color = vec3(0.0);
    
    Ray current_ray;
    current_ray.origin = ray.origin;
    current_ray.direction = ray.direction;

    for (int bounce = 0; bounce < max_bounces; bounce++) {
        HitRecord hit_rec;
        if (world_hit_aabb_stackless(current_ray, 0.001, infinity, hit_rec)) {
            Ray scattered;
            vec3 matColor;
            vec3 emitted = mats[hit_rec.mat_index].emission;

            if (scatter(state, current_ray, hit_rec, matColor, scattered)) {
                accumulated_color *= matColor;
                current_ray = scattered;

                // end early if contribution is below a threshold
                if (!(length(accumulated_color) > 0.001))
                    break;  
            } 
            else { // no scatter
                final_color += accumulated_color * matColor * emitted;
                break;
            }
        } else { // no hit
            vec3 unit_direction = normalize(current_ray.direction);
            float blend = 0.5 * (unit_direction.y + 1.0);
            vec3 background_color = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), blend);
            // vec3 background_color = vec3(0.05);

            final_color += accumulated_color * background_color;
            break;
        }
    }
    return final_color; // max depth reached
}


vec3 sample_square(uint state) {
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
    return vec3(RandomBilateral(state) - 0.5, RandomBilateral(state) - 0.5 , 0);
}


/** End of Ray **/

vec3 linear_to_srgb(vec3 linearColor) {
    return mix(
        linearColor * 12.92, 
        pow(linearColor, vec3(1.0 / 2.4)) * 1.055 - 0.055, 
        step(0.0031308, linearColor)
    );
}

float random_float(inout uint state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(state & 0x00FFFFFFu) / float(0x01000000);
}

vec2 sample_disk(inout uint state) {
    // Generate two random numbers in [0, 1)
    float u1 = RandomUnilateral(state);
    float u2 = RandomUnilateral(state);

    // Map to [-1, 1]
    float x = 2.0 * u1 - 1.0;
    float y = 2.0 * u2 - 1.0;

    // Handle degenerate case at origin
    if (x == 0.0 && y == 0.0) {
        return vec2(0.0);
    }

    float r, theta;
    if (abs(x) > abs(y)) {
        r = x;
        theta = (PI / 4.0) * (y / x);
    } else {
        r = y;
        theta = (PI / 2.0) - (PI / 4.0) * (x / y);
    }

    return r * vec2(cos(theta), sin(theta));
}


void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    uint width = uint(imageDimensions.x);
    uint height = uint(imageDimensions.y);

    uint random_state = time * (x + y * 36) + 1;

    vec2 inv_resolution = 1.0 / vec2(width, height);
    vec3 camera_right = vec3(invViewMatrix[0].xyz);
    vec3 camera_up    = vec3(invViewMatrix[1].xyz);
    float lens_radius = tan(radians(defocus_angle * 0.5)) * focus_distance;

    vec3 pixel_color = vec3(0.0);
    
    for (int s = 0; s < samples_per_pixel; ++s) {
        // Subpixel jitter
        vec2 offset = sample_square(random_state).xy;  
        vec2 uv = (vec2(pixel_coords) + offset) * inv_resolution;
        vec2 ndc = uv * 2.0 - 1.0;

        // World-space direction (view → clip → world)
        vec4 view_pos = invProjMatrix * vec4(ndc, -1.0, 1.0);
        view_pos /= view_pos.w;
        vec4 world_pos = invViewMatrix * view_pos;
        vec3 dir = normalize(world_pos.xyz - cameraPosition);

        // Sample lens disk and shift ray origin
        vec2 lens_sample = sample_disk(random_state) * lens_radius;
        vec3 lens_offset = camera_right * lens_sample.x + camera_up * lens_sample.y;
        vec3 origin = cameraPosition + lens_offset;

        // Recompute ray direction toward focal point
        vec3 focal_point = cameraPosition + dir * focus_distance;
        dir = normalize(focal_point - origin);

        Ray ray;
        ray.origin = origin;
        ray.direction = dir;

        pixel_color += ray_color2(ray, max_bounces, random_state);
    }


    vec3 prevColor = imageLoad(srcImage, pixel_coords).xyz;

    float scale_factor = 1.0 / float(samples_per_pixel);
    pixel_color *= scale_factor;
    pixel_color = linear_to_srgb(pixel_color);

    vec3 sum_color = prevColor * max(frameIndex, 0);
    vec3 final_color = (pixel_color + sum_color) / float(frameIndex + 1);

    imageStore(destImage, pixel_coords, vec4(final_color, 1.0));
}

