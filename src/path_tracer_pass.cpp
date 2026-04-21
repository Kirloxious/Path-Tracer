
#include "path_tracer_pass.h"
#include "compute_shader.h"

PathTracerPass::PathTracerPass(const std::filesystem::path& shaderPath) {
    shader = ComputeShader(shaderPath);
}

void PathTracerPass::uploadUniforms(const RenderContext& ctx) {
    shader.use();
    shader.setInt("num_objects", static_cast<int>(ctx.scene.world.spheres.size()));
    shader.setVec2("image_dimensions", glm::vec2(ctx.camera.image_width, ctx.camera.image_height));
    shader.setInt("bvh_size", static_cast<int>(ctx.scene.world.bvh.nodes.size()));
    shader.setInt("root_index", ctx.scene.world.bvh.root);
    shader.setInt("samples_per_pixel", ctx.camera.settings.samples_per_pixel);
    shader.setInt("max_bounces", ctx.camera.settings.max_bounces);
    shader.setInt("emissive_last_index", ctx.scene.world.emissiveLastIndex);
    shader.setInt("num_spheres", static_cast<int>(ctx.scene.world.spheres.size()));
    shader.setInt("num_triangles", static_cast<int>(ctx.scene.world.triangles.size()));
}

bool PathTracerPass::reloadIfChanged(RenderContext& ctx) {
    bool changed = shader.reloadIfChanged();
    if (changed) {
        uploadUniforms(ctx);
        ctx.resetFrameIndex();
    }
    return changed;
}

void PathTracerPass::execute(RenderContext& ctx, RenderTargets& targets) {
    ctx.frameIndex++;
    shader.use();
    shader.setInt("time", static_cast<int>(ctx.timeSeed++));
    shader.setInt("frame_index", ctx.frameIndex);

    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
