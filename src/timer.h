#pragma once

#include "buffer.h"
#include <GLFW/glfw3.h>
#include <string>

class Timer
{
public:
    virtual ~Timer() = default;

    virtual void start() = 0;
    virtual void end() = 0;

    virtual std::string formatted() = 0;

    Timer() = default;
};

class GPUTimer : public Timer
{

public:
    GPUTimer() {
        glGenQueries(2, queryIDs);
        for (int i = 0; i < 2; ++i) {
            glBeginQuery(GL_TIME_ELAPSED, queryIDs[i]);
            glEndQuery(GL_TIME_ELAPSED);
        }
    }

    ~GPUTimer() override {
        if (queryIDs[0]) {
            glDeleteQueries(2, queryIDs);
        }
    }

    void start() override { glBeginQuery(GL_TIME_ELAPSED, queryIDs[queryFrame]); }
    void end() override {
        glEndQuery(GL_TIME_ELAPSED);

        int prevQuery = 1 - queryFrame;
        glGetQueryObjectui64v(queryIDs[prevQuery], GL_QUERY_RESULT, &lastComputeTime);
        queryFrame = prevQuery;
    }

    std::string formatted() override {

        // Log::info("FPS: {} | Frame: {:.2f} ms | Compute: {:.2f} ms", frameCount, 1000.0 / frameCount, lastComputeTime / 1e6);
        return std::format("Compute: {:.2f} ms", lastComputeTime / 1e6);
    }

    double computeTimeMs() { return lastComputeTime / 1e6; }

private:
    GLuint   queryIDs[2]{};
    int      queryFrame = 0;
    GLuint64 lastComputeTime = 0;
};

class FPSTimer : public Timer
{
public:
    double deltaTime = 0;

    FPSTimer() = default;
    ~FPSTimer() {}

    void start() override {
        lastTime = glfwGetTime();
        timer = lastTime;
    }
    void end() override {

        currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        ++frameCount;
    }

    std::string formatted() override { return std::format("FPS: {} | Frame: {:.2f} ms", frameCount, 1000.0 / frameCount); }
    void        frameCountReset() {
        frameCount = 0;
        timer = currentTime;
    }
    double fps() { return frameCount; }
    double frameTimeMs() { return 1000.0 / frameCount; }

    int    frameCount = 0;
    double currentTime = 0;
    double lastTime = 0;
    double timer = 0;
};
