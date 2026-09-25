#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/gl_handle.h"

/// Frames of queries in flight before one is read back, so reads never stall on the GPU.
inline constexpr int TIMER_FRAMES_IN_FLIGHT = 4;

/// Reports a one-second running average; historyData() keeps the raw samples for the GUI plot.
class GPUTimer
{
public:
    GPUTimer() {
        for (QueryHandle& q : queries) {
            GLuint id = 0;
            glGenQueries(1, &id);
            q.reset(id);
        }
        lastSnapshot = glfwGetTime();
    }

    static constexpr int HISTORY = 120;

    void start() const { glBeginQuery(GL_TIME_ELAPSED, queries[write].get()); }

    /// Non-blocking readback: a query still pending is skipped rather than stalling the CPU.
    void end() {
        glEndQuery(GL_TIME_ELAPSED);
        hasResult[write] = true;

        // Advance unconditionally: leaving `write` in place would make the next start() re-begin
        // the query just ended and wedge the ring.
        write = (write + 1) % TIMER_FRAMES_IN_FLIGHT;

        const int read = write;
        if (!hasResult[read]) {
            return; // still filling the ring during the first few frames
        }
        GLuint available = GL_FALSE;
        glGetQueryObjectuiv(queries[read].get(), GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE) {
            return; // keep the previous sample rather than stalling
        }
        glGetQueryObjectui64v(queries[read].get(), GL_QUERY_RESULT, &lastComputeTime);

        history[historyOffset] = static_cast<float>(lastComputeTime / 1e6);
        historyOffset = (historyOffset + 1) % HISTORY;

        accumNs += lastComputeTime;
        ++accumCount;

        const double now = glfwGetTime();
        if (now - lastSnapshot >= snapshotInterval) {
            displayComputeMs = (accumNs / 1e6) / static_cast<double>(accumCount);
            accumNs = 0;
            accumCount = 0;
            lastSnapshot = now;
        }
    }

    double                          computeTimeMs() const { return displayComputeMs; }
    std::span<const float, HISTORY> historyData() const { return history; }
    int                             historyOffsetIndex() const { return historyOffset; }

private:
    std::array<QueryHandle, TIMER_FRAMES_IN_FLIGHT> queries;
    std::array<bool, TIMER_FRAMES_IN_FLIGHT>        hasResult{};
    int                                             write = 0;
    GLuint64                                        lastComputeTime = 0;

    double                  displayComputeMs = 0.0;
    GLuint64                accumNs = 0;
    int                     accumCount = 0;
    double                  lastSnapshot = 0.0;
    static constexpr double snapshotInterval = 1.0;

    std::array<float, HISTORY> history{};
    int                        historyOffset = 0;
};

/// Uses glQueryCounter, not GL_TIME_ELAPSED, to avoid nesting inside GPUTimer's active query.
class PassTimings
{
public:
    /// Queries are created on demand so the pass count is never capped.
    void addPass(std::string_view name) {
        Pass pass;
        pass.name = name;
        for (Slot& slot : pass.slots) {
            GLuint ids[2] = {};
            glGenQueries(2, ids);
            slot.qStart.reset(ids[0]);
            slot.qEnd.reset(ids[1]);
        }
        passes.push_back(std::move(pass));
    }

    /// Call at the start of Renderer::render().
    void beginFrame() {
        const int read = readIndex();
        for (Pass& pass : passes) {
            Slot& slot = pass.slots[read];
            if (!slot.hasResult) {
                continue; // still filling the ring during the first few frames
            }
            GLuint available = GL_FALSE;
            glGetQueryObjectuiv(slot.qEnd.get(), GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == GL_FALSE) {
                continue; // keep the previous sample rather than stalling
            }
            GLuint64 tStart = 0;
            GLuint64 tEnd = 0;
            glGetQueryObjectui64v(slot.qStart.get(), GL_QUERY_RESULT, &tStart);
            glGetQueryObjectui64v(slot.qEnd.get(), GL_QUERY_RESULT, &tEnd);
            // EWMA, alpha = 0.1.
            const double ms = (tEnd > tStart) ? static_cast<double>(tEnd - tStart) / 1e6 : 0.0;
            pass.ms = pass.ms * 0.9 + ms * 0.1;
        }
    }

    void beginPass(int idx) {
        if (!inRange(idx)) {
            return;
        }
        glQueryCounter(passes[static_cast<size_t>(idx)].slots[write].qStart.get(), GL_TIMESTAMP);
    }

    void endPass(int idx) {
        if (!inRange(idx)) {
            return;
        }
        Slot& slot = passes[static_cast<size_t>(idx)].slots[write];
        glQueryCounter(slot.qEnd.get(), GL_TIMESTAMP);
        slot.hasResult = true;
    }

    void endFrame() { write = (write + 1) % TIMER_FRAMES_IN_FLIGHT; }

    int              count() const { return static_cast<int>(passes.size()); }
    std::string_view nameFor(int i) const { return passes[static_cast<size_t>(i)].name; }
    double           msFor(int i) const { return passes[static_cast<size_t>(i)].ms; }

private:
    struct Slot
    {
        QueryHandle qStart;
        QueryHandle qEnd;
        bool        hasResult = false;
    };

    struct Pass
    {
        std::string                              name;
        double                                   ms = 0.0;
        std::array<Slot, TIMER_FRAMES_IN_FLIGHT> slots{};
    };

    bool inRange(int idx) const { return idx >= 0 && idx < count(); }

    /// The slot about to be overwritten, and so the oldest, whose queries have long retired.
    int readIndex() const { return (write + 1) % TIMER_FRAMES_IN_FLIGHT; }

    std::vector<Pass> passes;
    int               write = 0;
};

class FPSTimer
{
public:
    static constexpr int HISTORY = 120;

    double deltaTime = 0;

    FPSTimer() = default;

    void start() {
        lastTime = glfwGetTime();
        timer = lastTime;
    }
    void end() {
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

    double                          fps() const { return displayFps; }
    double                          frameTimeMs() const { return displayFrameMs; }
    std::span<const float, HISTORY> historyData() const { return history; }
    int                             historyOffsetIndex() const { return historyOffset; }

    int    frameCount = 0;
    double currentTime = 0;
    double lastTime = 0;
    double timer = 0;

private:
    double                  displayFps = 0.0;
    double                  displayFrameMs = 0.0;
    static constexpr double snapshotInterval = 1.0;

    std::array<float, HISTORY> history{};
    int                        historyOffset = 0;
};
