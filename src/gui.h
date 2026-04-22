#pragma once

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "window.h"

class Gui
{
    Gui(Window&);
    ~Gui();

    void beginFrame();
    void endFrame();
};
