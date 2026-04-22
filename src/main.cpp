#include "application.h"
#include "log.h"
#include "scene.h"

int main() {
    Log::info("Starting Application");
    Application app(Scene::CornellBox());
    return app.run();
}
