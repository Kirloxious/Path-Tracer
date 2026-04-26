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

    static Scene CornellBox();
    static Scene SphereWorld();
    static Scene Showcase();
};

using SceneFactory = Scene (*)();

struct SceneEntry
{
    std::string  name;
    SceneFactory factory;
};

std::vector<SceneEntry> sceneRegistry();
