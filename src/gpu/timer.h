#pragma once

#include "gpu/buffer.h"
#include <GLFW/glfw3.h>
#include <array>
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

// Per-pass GPU timings via GL_TIMESTAMP query pairs. Double-buffered so we read
// last frame's results at the start of this frame without stalling on any
// still-in-flight query. Uses `glQueryCounter` instead of GL_TIME_ELAPSED to
// avoid the nested-active-query conflict with the outer GPUTimer that already
// wraps Renderer::render().
class PassTimings
{
public:
    static constexpr int MAX_PASSES = 8;

    PassTimings() {
        for (int f = 0; f < 2; ++f) {
            for (int p = 0; p < MAX_PASSES; ++p) {
                glGenQueries(1, &slots[f][p].qStart);
                glGenQueries(1, &slots[f][p].qEnd);
                // Prime once so the first read returns "available"; without this the
                // first-frame read blocks or returns garbage on some drivers.
                glQueryCounter(slots[f][p].qStart, GL_TIMESTAMP);
                glQueryCounter(slots[f][p].qEnd, GL_TIMESTAMP);
            }
        }
    }

    ~PassTimings() {
        for (int f = 0; f < 2; ++f) {
            for (int p = 0; p < MAX_PASSES; ++p) {
                if (slots[f][p].qStart) glDeleteQueries(1, &slots[f][p].qStart);
                if (slots[f][p].qEnd) glDeleteQueries(1, &slots[f][p].qEnd);
            }
        }
    }

    // Registered once during Renderer setup, once per RenderPass.
    void addPass(const char* name) {
        if (passCount >= MAX_PASSES) return;
        passNames[passCount++] = name;
    }

    // Called at the start of Renderer::render(). Pulls in last frame's results.
    void beginFrame() {
        for (int p = 0; p < passCount; ++p) {
            GLuint64 tStart = 0;
            GLuint64 tEnd   = 0;
            glGetQueryObjectui64v(slots[read][p].qStart, GL_QUERY_RESULT, &tStart);
            glGetQueryObjectui64v(slots[read][p].qEnd, GL_QUERY_RESULT, &tEnd);
            // EWMA smoothing (α = 0.1) — same feel as GPUTimer's second-window average
            // without needing a wall-clock snapshot loop.
            const double ms = (tEnd > tStart) ? static_cast<double>(tEnd - tStart) / 1e6 : 0.0;
            passMs[p]       = passMs[p] * 0.9 + ms * 0.1;
        }
    }

    void beginPass(int idx) {
        if (idx < 0 || idx >= passCount) return;
        glQueryCounter(slots[write][idx].qStart, GL_TIMESTAMP);
    }

    void endPass(int idx) {
        if (idx < 0 || idx >= passCount) return;
        glQueryCounter(slots[write][idx].qEnd, GL_TIMESTAMP);
    }

    // Advance ping-pong at end of frame — swap the buffer we'll read next time.
    void endFrame() { std::swap(read, write); }

    int         count() const { return passCount; }
    const char* nameFor(int i) const { return passNames[i]; }
    double      msFor(int i) const { return passMs[i]; }

private:
    struct Slot
    {
        GLuint qStart = 0;
        GLuint qEnd   = 0;
    };
    std::array<std::array<Slot, MAX_PASSES>, 2> slots{};
    int                                         write     = 0;
    int                                         read      = 1;
    int                                         passCount = 0;
    const char*                                 passNames[MAX_PASSES] = {};
    double                                      passMs[MAX_PASSES]    = {};
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
