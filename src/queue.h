#pragma once

#include <cstdint>

#include <glad/glad.h>

#include "buffer.h"

// Counters for every wavefront queue, packed into a single SSBO so we stay
// under GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS. Mirrors `q_count[NUM_QUEUES]` in
// shader/common/queue.glsl.
class QueueCounters
{
public:
    QueueCounters() = default;

    QueueCounters(GLuint binding, int numSlots) {
        buffer = Buffer(GL_SHADER_STORAGE_BUFFER, binding, nullptr, numSlots * sizeof(uint32_t), GL_DYNAMIC_COPY);
        clearAll();
    }

    void clearAll() const {
        const uint32_t zero = 0;
        glClearNamedBufferData(buffer.id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    }

    void clearSlot(int slot) const {
        const uint32_t zero = 0;
        glNamedBufferSubData(buffer.id, slot * sizeof(uint32_t), sizeof(uint32_t), &zero);
    }

private:
    Buffer buffer;
};

// One wavefront queue. Owns the indices SSBO; the counter lives at slot
// `counterSlot` of a shared QueueCounters buffer.
struct Queue
{
    Buffer         indices;
    QueueCounters* counters    = nullptr;
    int            counterSlot = 0;

    Queue() = default;

    Queue(QueueCounters& sharedCounters, int slot, GLuint indicesBinding, size_t capacity)
        : counters(&sharedCounters), counterSlot(slot) {
        indices = Buffer(GL_SHADER_STORAGE_BUFFER, indicesBinding, nullptr, capacity * sizeof(uint32_t), GL_DYNAMIC_COPY);
    }

    void clear() const { counters->clearSlot(counterSlot); }
};
