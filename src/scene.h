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

    static std::vector<Scene> all();
};
