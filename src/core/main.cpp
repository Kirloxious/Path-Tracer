#include "core/application.h"
#include "core/log.h"

int main() {
    Log::info("Starting Application");
    Application app;
    return app.run();
}
