#include "gui.h"
#include "imgui.h"

#include "camera.h"
#include "scene.h"
#include "timer.h"
#include "world.h"

namespace Gui {

void init(Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window.window, true); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

void shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// Call after blitting to the swapchain so the overlay draws on top.
void endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void drawPerformance(const FPSTimer& fps, const GPUTimer& gpu) {
    ImVec4 fpsColor = fps.fps() >= 60.0   ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                      : fps.fps() >= 30.0 ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
                                          : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    ImGui::TextColored(fpsColor, "%3.0f FPS", fps.fps());
    ImGui::SameLine();
    ImGui::TextDisabled("(%.2f ms)", fps.frameTimeMs());

    const ImVec2 plotSize(220.0f, 36.0f);

    ImGui::PlotLines("##frametime", fps.historyData(), FPSTimer::HISTORY, fps.historyOffsetIndex(), nullptr, 0.0f, 33.33f, plotSize);

    ImGui::Text("Compute  %6.2f ms", gpu.computeTimeMs());
    ImGui::PlotLines("##compute", gpu.historyData(), GPUTimer::HISTORY, gpu.historyOffsetIndex(), nullptr, 0.0f, 33.33f, plotSize);
}

void drawSceneSwitcher(const std::vector<SceneEntry>& entries, SceneSwitchState& state) {
    if (entries.empty()) {
        return;
    }

    std::vector<const char*> names;
    names.reserve(entries.size());
    for (const auto& e : entries) {
        names.push_back(e.name.c_str());
    }

    ImGui::SetNextItemWidth(180.0f);
    int displayIdx = (state.requested >= 0) ? state.requested : state.current;
    if (ImGui::Combo("Scene", &displayIdx, names.data(), static_cast<int>(names.size()))) {
        if (displayIdx != state.current) {
            state.requested = displayIdx;
        }
    }
}

void drawScene(const Scene& scene) {
    ImGui::Text("Triangles  %zu", scene.world.triangles.size());
}

void drawCamera(const Camera& camera) {
    const auto& p = camera.data.lookfrom;
    const auto& f = camera.forward;
    ImGui::Text("Pos        %6.2f %6.2f %6.2f", p.x, p.y, p.z);
    ImGui::Text("Fwd        %6.2f %6.2f %6.2f", f.x, f.y, f.z);
    ImGui::Text("Yaw/Pitch  %6.1f %6.1f", camera.yaw, camera.pitch);
    ImGui::Text("FOV        %6.1f", camera.settings.vfov);
}

void drawStats(const FPSTimer& fps, const GPUTimer& gpu, const Scene& scene, const Camera& camera, const std::vector<SceneEntry>& sceneEntries,
               SceneSwitchState& sceneSwitch) {
    const float          pad = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + pad, vp->WorkPos.y + pad), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.40f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));

    if (ImGui::Begin("##perf-overlay", nullptr, flags)) {
        drawPerformance(fps, gpu);
        ImGui::Separator();
        drawSceneSwitcher(sceneEntries, sceneSwitch);
        drawScene(scene);
        ImGui::Separator();
        drawCamera(camera);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}
} // namespace Gui
