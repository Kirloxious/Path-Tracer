#pragma once

/**
 * @file scene.h
 * @brief Scene aggregate, the built-in scene factories, and the GUI-facing registry.
 */

#include <string>
#include <vector>

#include "scene/camera.h"
#include "scene/world.h"

/**
 * @brief Everything needed to render one scene: geometry, camera setup and environment.
 *
 * Produced entirely by the static factories below. Each factory builds a World, calls
 * World::sortEmissiveFirst() and then World::create(), and returns the finished Scene by
 * value.
 */
struct Scene
{
    std::string    name; ///< Display name; also becomes the window title.
    CameraSettings cameraSettings;
    World          world;

    /// Optional HDR equirect envmap. Empty path → no envmap (renders a black sky).
    /// Populated by scene factories (see SphereWorldEnvLit / ShowcaseEnvLit).
    std::string envMapPath;
    /// Multiplier applied to sampled environment radiance. Ignored when `envMapPath` is empty.
    float envIntensity = 1.0f;

    /// @return The Cornell box: red/green/white diffuse walls, two boxes, an emissive ceiling
    ///         sphere, plus a metal bunny and a glass Suzanne.
    static Scene CornellBox();
    /// @return An open sphere field exercising all four material types, lit by emissive spheres.
    static Scene SphereWorld();
    /// @return SphereWorld() lit by an HDR environment map instead of emissive geometry.
    static Scene SphereWorldEnvLit();
    /// @return A denser material and geometry showcase: bunny, Spot, Suzanne and dragon OBJ
    ///         meshes under a single emissive sphere.
    static Scene Showcase();
    /// @return Showcase() lit by an HDR environment map.
    static Scene ShowcaseEnvLit();
    /// @return A mirror metal floor and back wall with OBJ subjects under one emissive sphere —
    ///         a stress test for specular bounces.
    static Scene MirrorFloor();
    /// @return Parameter sweeps across the metallic-roughness model: conductor roughness,
    ///         dielectric roughness, metallic, transmission roughness and IOR, one row each,
    ///         lit by an envmap plus a warm key and a cool fill.
    static Scene MaterialGallery();
};

/// Pointer to one of the Scene static factories.
using SceneFactory = Scene (*)();

/// @brief One entry in the GUI's scene selector.
struct SceneEntry
{
    std::string  name;
    SceneFactory factory;
};

/**
 * @brief Lists every built-in scene for the GUI selector.
 *
 * The factories are not invoked here — a scene is only built when the user picks it and
 * Application::applyPendingSceneSwitch() runs.
 *
 * @return Name/factory pairs in menu order.
 */
std::vector<SceneEntry> sceneRegistry();
