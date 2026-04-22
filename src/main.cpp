#include "application.h"
#include "log.h"
#include "scene.h"

int main() {
    Log::info("path-tracer starting");
    Application app(Scene::CornellBox());
    return app.run();
}
