#pragma once

#include <string>
#include <vector>

#include "scene/camera.h"
#include "scene/world.h"

struct Scene
{
    std::string    name; ///< Also the window title.
    CameraSettings cameraSettings;
    World          world;

    /// Empty means no envmap (black sky).
    std::string envMapPath;
    float       envIntensity = 1.0f;

    static Scene CornellBox();
    static Scene SphereWorld();
    static Scene SphereWorldEnvLit();
    static Scene Showcase();
    static Scene ShowcaseEnvLit();
    static Scene MirrorFloor();
    static Scene MaterialGallery();
};

using SceneFactory = Scene (*)();

struct SceneEntry
{
    std::string  name;
    SceneFactory factory;
};

/// Factories are not invoked here; a scene is built only when picked.
std::vector<SceneEntry> sceneRegistry();
