#include "render/renderer.h"

#include <memory>
#include <stdexcept>

#include "core/log.h"
#include "core/shader_shared.h"
#include "gpu/buffer.h"
#include "render/gpu_constants.h"
#include "render/render_pass.h"
#include "render/render_targets.h"

Renderer::Renderer(int w, int h) : targets(w, h) {
    Log::info("Renderer");
    frameUBO = Buffer(FrameConstants{}, GL_DYNAMIC_DRAW);
    sceneUBO = Buffer(SceneConstants{}, GL_DYNAMIC_DRAW);
}

void Renderer::loadScene(const Scene& scene, const Camera& camera) {
    const World& world = scene.world;

    // The only step that can fail, so it runs first: a throw leaves the previous scene intact.
    EnvMap nextEnvMap;
    if (!scene.envMapPath.empty()) {
        auto loaded = EnvMap::load(scene.envMapPath, scene.envIntensity);
        if (!loaded) {
            throw std::runtime_error(loaded.error());
        }
        nextEnvMap = std::move(*loaded);
    }
    envMap = std::move(nextEnvMap);

    // An unlit scene still gets one zeroed group, so binding 0 never keeps the previous
    // scene's lights; the shaders gate every read on num_light_groups.
    const std::vector<World::LightGroup> noLights(1);
    lightGroupsSSBO = Buffer(world.lightGroups.empty() ? noLights : world.lightGroups, GL_STATIC_DRAW);
    matsSSBO = Buffer(world.materials, GL_STATIC_DRAW);
    camUBO = Buffer(camera.data, GL_DYNAMIC_DRAW);
    bvhNodesSSBO = Buffer(world.bvh.nodes, GL_STATIC_DRAW);
    trianglesSSBO = Buffer(world.triangles, GL_STATIC_DRAW);
    verticesSSBO = Buffer(world.vertices, GL_STATIC_DRAW);
    // A BVH leaf owns a contiguous run here, each entry indexing trianglesSSBO. The indirection
    // lets leaves batch triangles without disturbing the emissive-first triangle order.
    triRefsSSBO = Buffer(world.bvh.triRefs, GL_STATIC_DRAW);

    // Uploaded even without an envmap: every read is gated on env_map_valid, but an empty
    // binding would make a stray read undefined rather than merely wrong.
    const std::vector<EnvSampleCell> fallback(1);
    envSamplingSSBO = Buffer(envMap.samplingCells().empty() ? fallback : envMap.samplingCells(), GL_STATIC_DRAW);

    const SceneConstants constants{
        .bvh_root_index = world.bvh.root,
        .emissive_last_index = world.emissiveLastIndex,
        .num_light_groups = static_cast<int32_t>(world.lightGroups.size()),
        .max_bounces = camera.settings.max_bounces,
        .indirect_clamp = camera.settings.indirect_clamp,
        .env_map_intensity = scene.envIntensity,
        .env_sample_size = envMap.valid() ? envMap.samplingSize() : glm::ivec2(0),
        .env_map_valid = envMap.valid() ? 1 : 0,
    };
    sceneUBO.update(constants);

    Log::info("Renderer: Buffers created");
    for (auto& pass : passes) {
        pass->onSceneLoaded(scene);
    }

    Log::info("Renderer: Scene loaded.");
}

void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    targets.resize(w, h);
    for (auto& pass : passes) {
        pass->resize(w, h);
    }
}

void Renderer::updateCameraUbo(const Camera& cam) {
    camUBO.update(cam.data);
}

void Renderer::bindSceneResources() const {
    lightGroupsSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_LIGHT_GROUPS);
    matsSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS);
    bvhNodesSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_BVH_NODES);
    trianglesSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_TRIANGLES);
    verticesSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_VERTICES);
    triRefsSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_TRI_REFS);
    envSamplingSSBO.bindBase(GL_SHADER_STORAGE_BUFFER, BIND_ENV_SAMPLES);

    camUBO.bindBase(GL_UNIFORM_BUFFER, UBO_CAMERA);
    frameUBO.bindBase(GL_UNIFORM_BUFFER, UBO_FRAME);
    sceneUBO.bindBase(GL_UNIFORM_BUFFER, UBO_SCENE);

    envMap.bind(TEX_ENV_MAP);
}

void Renderer::render(const RenderContext& ctx) {
    const FrameConstants frame{
        .image_size = {targets.width, targets.height},
        .frame_index = ctx.frameIndex,
        .history_frames = ctx.historyFrames,
        .time_seed = ctx.timeSeed,
        .run_seed = ctx.runSeed,
    };
    frameUBO.update(frame);
    bindSceneResources();

    // Pull in previous frame's per-pass timestamps before we overwrite them.
    passTimings.beginFrame();
    for (size_t i = 0; i < passes.size(); ++i) {
        passTimings.beginPass(static_cast<int>(i));
        passes[i]->execute(ctx, targets);
        passTimings.endPass(static_cast<int>(i));
    }
    passTimings.endFrame();
}

bool Renderer::reloadShadersIfChanged() {
    bool changed = false;
    for (auto& pass : passes) {
        changed |= pass->reloadIfChanged();
    }
    return changed;
}

void Renderer::addRenderPass(std::unique_ptr<RenderPass> pass) {
    passTimings.addPass(pass->name());
    passes.push_back(std::move(pass));
}

void Renderer::blitToSwapChain(int width, int height) {
    targets.fb.blit(width, height);
}

void Renderer::blitGBufferAttachmentToSwapChain(int attachmentIndex, int width, int height) {
    targets.gbuf.blitAttachmentToSwapChain(attachmentIndex, width, height);
}
