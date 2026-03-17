#include "application.h"
#include "scene.h"

int main() {
    Application app(Scene::CornellBox());
    return app.run();
}
