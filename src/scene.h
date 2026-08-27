#pragma once

#include <string>
#include <vector>

#include "camera.h"
#include "world.h"

struct Scene
{
    std::string    name;
    CameraSettings cameraSettings;
    World          world;

    // Optional HDR equirect envmap. Empty path → no envmap (renders black sky).
    // Populated by scene factories (see SphereWorldEnvLit / ShowcaseEnvLit).
    std::string envMapPath;
    float       envIntensity = 1.0f;

    static Scene CornellBox();
    static Scene SphereWorld();
    static Scene SphereWorldEnvLit();
    static Scene Showcase();
    static Scene ShowcaseEnvLit();
    static Scene MirrorFloor();
};

using SceneFactory = Scene (*)();

struct SceneEntry
{
    std::string  name;
    SceneFactory factory;
};

std::vector<SceneEntry> sceneRegistry();
