#include <exception>

#include "core/application.h"
#include "core/log.h"

int main() {
    try {
        Log::info("Starting Application");
        Application app;
        return app.run();
    } catch (const std::exception& e) {
        Log::error("Fatal: {}", e.what());
        return 1;
    }
}
