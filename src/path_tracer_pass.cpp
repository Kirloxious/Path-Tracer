
#include "path_tracer_pass.h"
#include "compute_shader.h"
#include "frame_buffer.h"
#include "log.h"
#include "shader_program.h"

PathTracerPass::PathTracerPass(const std::filesystem::path& shaderPath) {
    Log::info("PathTracerPass: loading '{}'", shaderPath.string());
    shader = ComputeShader(shaderPath);
}

void PathTracerPass::uploadUniforms(const RenderContext& ctx) {
    shader.use();
    shader.setVec2("image_dimensions", glm::vec2(ctx.camera.image_width, ctx.camera.image_height));
    shader.setInt("root_index", ctx.scene.world.bvh.root);
    shader.setInt("samples_per_pixel", ctx.camera.settings.samples_per_pixel);
    shader.setInt("max_bounces", ctx.camera.settings.max_bounces);
    shader.setInt("emissive_last_index", ctx.scene.world.emissiveLastIndex);
    shader.setInt("num_light_groups", static_cast<int>(ctx.scene.world.lightGroups.size()));
}

bool PathTracerPass::reloadIfChanged(const RenderContext& ctx) {
    bool changed = shader.reloadIfChanged();
    if (changed) {
        uploadUniforms(ctx);
    }
    return changed;
}

void PathTracerPass::execute(const RenderContext& ctx, RenderTargets& targets) {
    shader.use();
    shader.setInt("time", static_cast<int>(ctx.timeSeed));
    shader.setInt("frame_index", ctx.frameIndex);

    targets.accum.bindForAccumulation();
    targets.normals.bind(2, GL_WRITE_ONLY);

    glBindTextureUnit(5, targets.gbuf.pos_matid.handle);
    glBindTextureUnit(6, targets.gbuf.normal.handle);

    glDispatchCompute(targets.numGroupsX, targets.numGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
