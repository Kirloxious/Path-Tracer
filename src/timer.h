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
        lastSnapshot = glfwGetTime();
    }

    ~GPUTimer() override {
        if (queryIDs[0]) {
            glDeleteQueries(2, queryIDs);
        }
    }

    static constexpr int HISTORY = 120;

    void start() override { glBeginQuery(GL_TIME_ELAPSED, queryIDs[queryFrame]); }
    void end() override {
        glEndQuery(GL_TIME_ELAPSED);

        int prevQuery = 1 - queryFrame;
        glGetQueryObjectui64v(queryIDs[prevQuery], GL_QUERY_RESULT, &lastComputeTime);
        queryFrame = prevQuery;

        float ms = static_cast<float>(lastComputeTime / 1e6);
        history[historyOffset] = ms;
        historyOffset = (historyOffset + 1) % HISTORY;

        accumNs += lastComputeTime;
        ++accumCount;

        double now = glfwGetTime();
        if (now - lastSnapshot >= snapshotInterval) {
            displayComputeMs = (accumNs / 1e6) / static_cast<double>(accumCount);
            accumNs = 0;
            accumCount = 0;
            lastSnapshot = now;
        }
    }

    std::string formatted() override { return std::format("Compute: {:.2f} ms", displayComputeMs); }

    double       computeTimeMs() const { return displayComputeMs; }
    const float* historyData() const { return history; }
    int          historyOffsetIndex() const { return historyOffset; }

private:
    GLuint   queryIDs[2]{};
    int      queryFrame = 0;
    GLuint64 lastComputeTime = 0;

    double                  displayComputeMs = 0.0;
    GLuint64                accumNs = 0;
    int                     accumCount = 0;
    double                  lastSnapshot = 0.0;
    static constexpr double snapshotInterval = 1.0;

    float history[HISTORY] = {};
    int   historyOffset = 0;
};

class FPSTimer : public Timer
{
public:
    static constexpr int HISTORY = 120;

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

        history[historyOffset] = static_cast<float>(deltaTime * 1000.0);
        historyOffset = (historyOffset + 1) % HISTORY;

        double elapsed = currentTime - timer;
        if (elapsed >= snapshotInterval) {
            displayFps = frameCount / elapsed;
            displayFrameMs = 1000.0 * elapsed / frameCount;
            frameCount = 0;
            timer = currentTime;
        }
    }

    std::string formatted() override { return std::format("FPS: {:.0f} | Frame: {:.2f} ms", displayFps, displayFrameMs); }

    double       fps() const { return displayFps; }
    double       frameTimeMs() const { return displayFrameMs; }
    const float* historyData() const { return history; }
    int          historyOffsetIndex() const { return historyOffset; }

    int    frameCount = 0;
    double currentTime = 0;
    double lastTime = 0;
    double timer = 0;

private:
    double                  displayFps = 0.0;
    double                  displayFrameMs = 0.0;
    static constexpr double snapshotInterval = 1.0;

    float history[HISTORY] = {};
    int   historyOffset = 0;
};
