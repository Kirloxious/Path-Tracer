#include "core/application.h"
#include "core/log.h"
#include "scene/scene.h"

int main() {
    Log::info("Starting Application");
    Application app(Scene::CornellBox());
    return app.run();
}
