#pragma once

/**
 * @file queue.h
 * @brief Host-side mirrors of the wavefront path tracer's GPU work queues.
 */

#include <cstdint>

#include <glad/glad.h>

#include "gpu/buffer.h"

/**
 * @brief The single SSBO holding one `uint` counter per wavefront queue.
 *
 * All queue counters share one buffer because NVIDIA caps
 * `GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS` at 16; bundling them keeps the pass under the limit.
 * Mirrors `q_count[NUM_QUEUES]` in `shader/common/queue.glsl`, indexed by the `Q_*` constants.
 */
class QueueCounters
{
public:
    QueueCounters() = default;

    /**
     * @brief Allocates the shared counter buffer and zeroes every slot.
     * @param binding  SSBO binding index (11 in the current layout).
     * @param numSlots Number of counters, one per queue.
     */
    QueueCounters(GLuint binding, int numSlots) {
        buffer = Buffer(GL_SHADER_STORAGE_BUFFER, binding, nullptr, numSlots * sizeof(uint32_t), GL_DYNAMIC_COPY);
        clearAll();
    }

    /// Zeroes every counter — done once per frame before the first dispatch.
    void clearAll() const {
        const uint32_t zero = 0;
        glClearNamedBufferData(buffer.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    }

    /**
     * @brief Zeroes a single counter, leaving the others intact.
     * @param slot Queue index (`Q_RAY` .. `Q_SHADOW`) whose counter to reset.
     */
    void clearSlot(int slot) const {
        const uint32_t zero = 0;
        glNamedBufferSubData(buffer.id, slot * sizeof(uint32_t), sizeof(uint32_t), &zero);
    }

private:
    Buffer buffer;
};

/**
 * @brief One wavefront queue: an indices SSBO plus a slot in the shared counter buffer.
 *
 * Shaders append with an atomic increment on `q_count[counterSlot]` and write the path index
 * into the indices buffer. The Queue does not own its counter — it points at a QueueCounters
 * that must outlive it.
 */
struct Queue
{
    Buffer         indices;
    QueueCounters* counters = nullptr;
    int            counterSlot = 0;

    Queue() = default;

    /**
     * @brief Allocates the indices SSBO and binds this queue to a shared counter slot.
     * @param sharedCounters Counter buffer to borrow a slot from; must outlive this Queue.
     * @param slot           Index of this queue's counter within @p sharedCounters.
     * @param indicesBinding SSBO binding index for the indices buffer.
     * @param capacity       Maximum entries — the worst case is one per pixel.
     */
    Queue(QueueCounters& sharedCounters, int slot, GLuint indicesBinding, size_t capacity) : counters(&sharedCounters), counterSlot(slot) {
        indices = Buffer(GL_SHADER_STORAGE_BUFFER, indicesBinding, nullptr, capacity * sizeof(uint32_t), GL_DYNAMIC_COPY);
    }

    /// Resets this queue to empty by zeroing its counter. Stale index data is left in place —
    /// entries past the counter are never read.
    void clear() const { counters->clearSlot(counterSlot); }
};
