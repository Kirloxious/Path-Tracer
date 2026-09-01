#pragma once

/**
 * @file timer.h
 * @brief GPU and CPU frame timers, ring-buffered to avoid pipeline stalls.
 */

#include "gpu/buffer.h"
#include <GLFW/glfw3.h>
#include <array>
#include <utility>
#include <vector>

/**
 * @brief How many frames of GPU queries to keep in flight before reading one back.
 *
 */
inline constexpr int TIMER_FRAMES_IN_FLIGHT = 4;

/**
 * @brief Whole-frame GPU time via a ring of `GL_TIME_ELAPSED` queries.
 *
 * Wraps Renderer::render(). The reported value is a one-second running average, and
 * historyData() additionally keeps the last HISTORY raw samples for the GUI plot.
 *
 * Owns raw GL query handles, so it is non-copyable.
 */
class GPUTimer
{
public:
    /// Allocates the ring of timer queries.
    GPUTimer() {
        glGenQueries(TIMER_FRAMES_IN_FLIGHT, queryIDs.data());
        lastSnapshot = glfwGetTime();
    }

    ~GPUTimer() {
        if (queryIDs[0]) {
            glDeleteQueries(TIMER_FRAMES_IN_FLIGHT, queryIDs.data());
        }
    }

    // Owns raw GL query handles; copying would double-free them.
    GPUTimer(const GPUTimer&) = delete;
    GPUTimer& operator=(const GPUTimer&) = delete;

    /// Number of raw per-frame samples retained for the GUI plot.
    static constexpr int HISTORY = 120;

    /// Begins the current ring slot's timer query. Pair with end().
    void start() const { glBeginQuery(GL_TIME_ELAPSED, queryIDs[write]); }

    /**
     * @brief Ends the current query, advances the ring, and reads back the oldest result.
     *
     * The read is non-blocking: by the time the ring wraps, the sampled query is three frames
     * old and long retired. If it is somehow still pending the sample is skipped rather than
     * stalling the CPU on the GPU.
     */
    void end() {
        glEndQuery(GL_TIME_ELAPSED);
        hasResult[write] = true;

        // Advance first: `write` then indexes the oldest slot in the ring, which is the one
        // we read. Advancing unconditionally matters — an early return that left `write` in
        // place would make the next start() re-begin the query we just ended, discarding it
        // and wedging the ring permanently.
        write = (write + 1) % TIMER_FRAMES_IN_FLIGHT;

        const int read = write;
        if (!hasResult[read]) {
            return; // still filling the ring during the first few frames
        }
        GLuint available = GL_FALSE;
        glGetQueryObjectuiv(queryIDs[read], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE) {
            return; // keep the previous sample rather than stalling
        }
        glGetQueryObjectui64v(queryIDs[read], GL_QUERY_RESULT, &lastComputeTime);

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

    /// @return GPU time for the render in milliseconds, averaged over the last second.
    double computeTimeMs() const { return displayComputeMs; }
    /// @return Pointer to HISTORY raw per-frame samples in milliseconds (a circular buffer).
    const float* historyData() const { return history; }
    /// @return Index of the oldest entry in historyData(), i.e. where the plot should start.
    int historyOffsetIndex() const { return historyOffset; }

private:
    std::array<GLuint, TIMER_FRAMES_IN_FLIGHT> queryIDs{};
    std::array<bool, TIMER_FRAMES_IN_FLIGHT>   hasResult{};
    int                                        write = 0;
    GLuint64                                   lastComputeTime = 0;

    double                  displayComputeMs = 0.0;
    GLuint64                accumNs = 0;
    int                     accumCount = 0;
    double                  lastSnapshot = 0.0;
    static constexpr double snapshotInterval = 1.0;

    float history[HISTORY] = {};
    int   historyOffset = 0;
};

/**
 * @brief Per-pass GPU timings via `GL_TIMESTAMP` query pairs.
 *
 * Ring-buffered TIMER_FRAMES_IN_FLIGHT deep so each read targets a long-retired frame (see
 * that constant's documentation). Uses `glQueryCounter` rather than `GL_TIME_ELAPSED` to
 * avoid the nested-active-query conflict with the outer GPUTimer that already wraps
 * Renderer::render(). Results are EWMA-smoothed for a readable GUI panel.
 *
 * Owns raw GL query handles, so it is non-copyable.
 */
class PassTimings
{
public:
    PassTimings() = default;

    ~PassTimings() {
        for (Pass& pass : passes) {
            for (Slot& slot : pass.slots) {
                if (slot.qStart) {
                    glDeleteQueries(1, &slot.qStart);
                }
                if (slot.qEnd) {
                    glDeleteQueries(1, &slot.qEnd);
                }
            }
        }
    }

    // Owns raw GL query handles; copying would double-free them on destruction.
    PassTimings(const PassTimings&) = delete;
    PassTimings& operator=(const PassTimings&) = delete;

    /**
     * @brief Registers one pass and allocates its query pairs.
     *
     * Called once per RenderPass during Renderer::addRenderPass(). Queries are created on
     * demand so the pass count is never capped — a fixed cap silently dropped the last passes
     * from the panel whenever the pipeline grew.
     *
     * @param name Display name for the GUI panel. Must be a static string: it is stored by
     *             pointer, not copied.
     */
    void addPass(const char* name) {
        Pass pass;
        pass.name = name;
        for (Slot& slot : pass.slots) {
            glGenQueries(1, &slot.qStart);
            glGenQueries(1, &slot.qEnd);
        }
        passes.push_back(pass);
    }

    /**
     * @brief Pulls in the oldest ring frame's results. Call at the start of Renderer::render().
     *
     * Passes whose query is not yet available keep their previous sample rather than stalling.
     */
    void beginFrame() {
        const int read = readIndex();
        for (Pass& pass : passes) {
            Slot& slot = pass.slots[read];
            if (!slot.hasResult) {
                continue; // still filling the ring during the first few frames
            }
            GLuint available = GL_FALSE;
            glGetQueryObjectuiv(slot.qEnd, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == GL_FALSE) {
                continue; // keep the previous sample rather than stalling
            }
            GLuint64 tStart = 0;
            GLuint64 tEnd = 0;
            glGetQueryObjectui64v(slot.qStart, GL_QUERY_RESULT, &tStart);
            glGetQueryObjectui64v(slot.qEnd, GL_QUERY_RESULT, &tEnd);
            // EWMA smoothing (α = 0.1) — same feel as GPUTimer's second-window average
            // without needing a wall-clock snapshot loop.
            const double ms = (tEnd > tStart) ? static_cast<double>(tEnd - tStart) / 1e6 : 0.0;
            pass.ms = pass.ms * 0.9 + ms * 0.1;
        }
    }

    /**
     * @brief Writes the start timestamp for one pass. Out-of-range indices are ignored.
     * @param idx Index of the pass in registration order.
     */
    void beginPass(int idx) {
        if (!inRange(idx)) {
            return;
        }
        glQueryCounter(passes[static_cast<size_t>(idx)].slots[write].qStart, GL_TIMESTAMP);
    }

    /**
     * @brief Writes the end timestamp for one pass. Out-of-range indices are ignored.
     * @param idx Index of the pass in registration order; must match the beginPass() call.
     */
    void endPass(int idx) {
        if (!inRange(idx)) {
            return;
        }
        Slot& slot = passes[static_cast<size_t>(idx)].slots[write];
        glQueryCounter(slot.qEnd, GL_TIMESTAMP);
        slot.hasResult = true;
    }

    /// Advances the ring. Call once at the end of each frame.
    void endFrame() { write = (write + 1) % TIMER_FRAMES_IN_FLIGHT; }

    /// @return Number of registered passes.
    int count() const { return static_cast<int>(passes.size()); }
    /** @param i Pass index in [0, count()). @return The pass's display name. */
    const char* nameFor(int i) const { return passes[static_cast<size_t>(i)].name; }
    /** @param i Pass index in [0, count()). @return EWMA-smoothed GPU time in milliseconds. */
    double msFor(int i) const { return passes[static_cast<size_t>(i)].ms; }

private:
    /// One frame's query pair for one pass.
    struct Slot
    {
        GLuint qStart = 0;
        GLuint qEnd = 0;
        bool   hasResult = false;
    };

    /// One registered pass: its label, smoothed timing, and per-frame query slots.
    struct Pass
    {
        const char*                              name = nullptr;
        double                                   ms = 0.0;
        std::array<Slot, TIMER_FRAMES_IN_FLIGHT> slots{};
    };

    /// @param idx Candidate pass index. @return true when @p idx addresses a registered pass.
    bool inRange(int idx) const { return idx >= 0 && idx < count(); }

    /// @return The ring slot to read this frame — the one we are about to overwrite, and so
    ///         the oldest, whose queries have long since retired.
    int readIndex() const { return (write + 1) % TIMER_FRAMES_IN_FLIGHT; }

    std::vector<Pass> passes;
    int               write = 0;
};

/**
 * @brief Wall-clock frame timer: per-frame delta plus a one-second FPS average.
 *
 * `deltaTime` is read every frame by Application to drive camera movement and the
 * auto-exposure EMA; fps() / frameTimeMs() are the smoothed values shown in the GUI.
 */
class FPSTimer
{
public:
    /// Number of raw per-frame samples retained for the GUI plot.
    static constexpr int HISTORY = 120;

    /// Seconds elapsed since the previous end() call.
    double deltaTime = 0;

    FPSTimer() = default;

    /// Marks the start of the first frame and resets the averaging window.
    void start() {
        lastTime = glfwGetTime();
        timer = lastTime;
    }
    /// Closes the current frame: updates deltaTime, appends to the history plot, and
    /// refreshes the displayed FPS once per second.
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

    /// @return Frames per second, averaged over the last second.
    double fps() const { return displayFps; }
    /// @return Mean wall-clock frame time in milliseconds, averaged over the last second.
    double frameTimeMs() const { return displayFrameMs; }
    /// @return Pointer to HISTORY raw frame times in milliseconds (a circular buffer).
    const float* historyData() const { return history; }
    /// @return Index of the oldest entry in historyData(), i.e. where the plot should start.
    int historyOffsetIndex() const { return historyOffset; }

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
