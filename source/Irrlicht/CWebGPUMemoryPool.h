/*
 * WebGPU Memory Pool Architecture for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 *
 * Zero-allocation rendering with persistent GPU memory pools
 * Eliminates WASM heap fragmentation and allocation overhead
 */

#ifndef IRR_C_WEBGPU_MEMORY_POOL_H_INCLUDED
#define IRR_C_WEBGPU_MEMORY_POOL_H_INCLUDED

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include "IReferenceCounted.h"
#include "irrTypes.h"

#include <webgpu/webgpu.h>
#include <vector>
#include <deque>
#include <unordered_map>

namespace irr {
namespace video {

// Forward declarations
class CWebGPUDriver;

// Memory allocation descriptor
struct MemoryAllocation {
    WGPUBuffer buffer;
    u32 offset;
    u32 size;
    u32 alignment;
    void* mappedPtr;
    u32 frameAllocated;
    u32 frameReleased;
    bool inUse;
};

// Memory pool for specific buffer types
class CWebGPUBufferPool {
public:
    enum BufferType {
        BUFFER_TYPE_VERTEX = 0,
        BUFFER_TYPE_INDEX,
        BUFFER_TYPE_UNIFORM,
        BUFFER_TYPE_STORAGE,
        BUFFER_TYPE_INDIRECT,
        BUFFER_TYPE_COUNT
    };

    CWebGPUBufferPool(WGPUDevice device, BufferType type, u32 poolSize, u32 alignment = 256);
    ~CWebGPUBufferPool();

    // Allocation management
    MemoryAllocation allocate(u32 size, u32 alignment = 0);
    void deallocate(const MemoryAllocation& allocation);
    void advanceFrame();

    // Pool information
    u32 getTotalSize() const { return poolSize; }
    u32 getUsedSize() const { return usedSize; }
    u32 getFreeSize() const { return poolSize - usedSize; }
    f32 getUtilization() const { return (f32)usedSize / poolSize; }

    // Statistics
    struct PoolStats {
        u32 totalAllocations;
        u32 activeAllocations;
        u32 failedAllocations;
        u32 fragmentedSize;
        u32 largestFreeBlock;
        f32 averageAllocationSize;
        f32 fragmentation; // Percentage
    };

    const PoolStats& getStats() const { return stats; }
    void updateStats();

private:
    WGPUDevice device;
    BufferType type;
    WGPUBuffer megaBuffer;
    u8* mappedMemory;

    u32 poolSize;
    u32 usedSize;
    u32 defaultAlignment;
    u32 currentFrame;

    // Free block management
    struct FreeBlock {
        u32 offset;
        u32 size;
        u32 frameFreed;

        bool operator<(const FreeBlock& other) const {
            return size < other.size; // For size-based ordering
        }
    };

    std::vector<FreeBlock> freeBlocks;
    std::vector<MemoryAllocation> activeAllocations;

    PoolStats stats;

    // Internal methods
    bool createMegaBuffer();
    u32 findFreeBlock(u32 size, u32 alignment);
    void coalesceFreeBlocks();
    void splitBlock(u32 blockIndex, u32 size);
    WGPUBufferUsageFlags getBufferUsage() const;
    const char* getBufferLabel() const;

    // Alignment helpers
    u32 alignSize(u32 size, u32 alignment) const;
    bool isAligned(u32 offset, u32 alignment) const;
};

// Ring buffer for per-frame data
template<typename T, u32 FrameCount = 3>
class CWebGPURingBuffer {
public:
    CWebGPURingBuffer(WGPUDevice device, u32 elementsPerFrame)
        : device(device), elementsPerFrame(elementsPerFrame), currentFrame(0) {

        // Create buffer for all frames
        WGPUBufferDescriptor desc = {};
        desc.label = "Ring Buffer";
        desc.size = FrameCount * elementsPerFrame * sizeof(T);
        desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        buffer = wgpuDeviceCreateBuffer(device, &desc);

        // Initialize data storage
        data.resize(FrameCount * elementsPerFrame);
    }

    ~CWebGPURingBuffer() {
        if (buffer) wgpuBufferRelease(buffer);
    }

    // Frame management
    void advanceFrame() {
        currentFrame = (currentFrame + 1) % FrameCount;
    }

    // Data access
    T* getCurrentFrameData() {
        return &data[currentFrame * elementsPerFrame];
    }

    u32 getCurrentFrameOffset() const {
        return currentFrame * elementsPerFrame * sizeof(T);
    }

    // Upload current frame data to GPU
    void uploadCurrentFrame(WGPUQueue queue) {
        u32 offset = getCurrentFrameOffset();
        u32 size = elementsPerFrame * sizeof(T);
        wgpuQueueWriteBuffer(queue, buffer, offset, getCurrentFrameData(), size);
    }

    WGPUBuffer getBuffer() const { return buffer; }
    u32 getFrameSize() const { return elementsPerFrame * sizeof(T); }

private:
    WGPUDevice device;
    WGPUBuffer buffer;
    std::vector<T> data;
    u32 elementsPerFrame;
    u32 currentFrame;
};

// Main memory pool manager
class CWebGPUMemoryPool : public IReferenceCounted {
public:
    CWebGPUMemoryPool(CWebGPUDriver* driver);
    virtual ~CWebGPUMemoryPool();

    // Initialization
    bool initialize();
    bool isInitialized() const { return initialized; }

    // Pool configuration
    struct PoolConfig {
        u32 vertexPoolSize = 256 * 1024 * 1024;   // 256MB for vertex data
        u32 indexPoolSize = 64 * 1024 * 1024;     // 64MB for index data
        u32 uniformPoolSize = 16 * 1024 * 1024;   // 16MB for uniforms
        u32 storagePoolSize = 128 * 1024 * 1024;  // 128MB for storage buffers
        u32 indirectPoolSize = 1024 * 1024;       // 1MB for indirect commands
    };

    void setPoolConfig(const PoolConfig& config) { this->config = config; }
    const PoolConfig& getPoolConfig() const { return config; }

    // Memory allocation
    MemoryAllocation allocateVertexMemory(u32 size, u32 alignment = 16);
    MemoryAllocation allocateIndexMemory(u32 size, u32 alignment = 4);
    MemoryAllocation allocateUniformMemory(u32 size, u32 alignment = 256);
    MemoryAllocation allocateStorageMemory(u32 size, u32 alignment = 16);
    MemoryAllocation allocateIndirectMemory(u32 size, u32 alignment = 4);

    void deallocateMemory(const MemoryAllocation& allocation);

    // Frame management
    void beginFrame();
    void endFrame();
    void advanceFrame();

    // Ring buffers for per-frame data
    using TransformRingBuffer = CWebGPURingBuffer<core::matrix4, 3>;
    using MaterialRingBuffer = CWebGPURingBuffer<f32, 3>; // Material parameters
    using LightRingBuffer = CWebGPURingBuffer<f32, 3>;    // Light data

    TransformRingBuffer* getTransformRingBuffer() { return &transformRing; }
    MaterialRingBuffer* getMaterialRingBuffer() { return &materialRing; }
    LightRingBuffer* getLightRingBuffer() { return &lightRing; }

    // Statistics and monitoring
    struct MemoryStats {
        u32 totalPoolSize;
        u32 totalUsedSize;
        u32 totalFreeSize;
        f32 overallUtilization;
        u32 totalAllocations;
        u32 failedAllocations;
        f32 averageFragmentation;
        u32 frameAdvances;
    };

    const MemoryStats& getStats() const { return stats; }
    void updateStats();

    // Debug and profiling
    void setDebugMode(bool enable) { debugMode = enable; }
    void logMemoryUsage() const;
    void validatePools() const;

    // Compaction and cleanup
    void compactPools();
    void cleanupOldAllocations();

private:
    CWebGPUDriver* driver;
    WGPUDevice device;
    WGPUQueue queue;
    bool initialized;
    bool debugMode;

    PoolConfig config;
    u32 currentFrame;

    // Buffer pools
    CWebGPUBufferPool* vertexPool;
    CWebGPUBufferPool* indexPool;
    CWebGPUBufferPool* uniformPool;
    CWebGPUBufferPool* storagePool;
    CWebGPUBufferPool* indirectPool;

    // Ring buffers
    TransformRingBuffer transformRing;
    MaterialRingBuffer materialRing;
    LightRingBuffer lightRing;

    // Statistics
    MemoryStats stats;

    // Internal methods
    bool createPools();
    void destroyPools();
    CWebGPUBufferPool* getPoolForAllocation(const MemoryAllocation& allocation);

    // Memory tracking
    std::unordered_map<WGPUBuffer, CWebGPUBufferPool*> bufferToPool;
    std::vector<MemoryAllocation> activeAllocations;
};

// RAII wrapper for memory allocations
class CWebGPUMemoryBlock {
public:
    CWebGPUMemoryBlock(CWebGPUMemoryPool* pool, const MemoryAllocation& allocation)
        : pool(pool), allocation(allocation), released(false) {}

    ~CWebGPUMemoryBlock() {
        if (!released && pool) {
            pool->deallocateMemory(allocation);
        }
    }

    // Move semantics
    CWebGPUMemoryBlock(CWebGPUMemoryBlock&& other) noexcept
        : pool(other.pool), allocation(other.allocation), released(other.released) {
        other.released = true;
    }

    CWebGPUMemoryBlock& operator=(CWebGPUMemoryBlock&& other) noexcept {
        if (this != &other) {
            if (!released && pool) {
                pool->deallocateMemory(allocation);
            }
            pool = other.pool;
            allocation = other.allocation;
            released = other.released;
            other.released = true;
        }
        return *this;
    }

    // Disable copy
    CWebGPUMemoryBlock(const CWebGPUMemoryBlock&) = delete;
    CWebGPUMemoryBlock& operator=(const CWebGPUMemoryBlock&) = delete;

    // Access
    const MemoryAllocation& getAllocation() const { return allocation; }
    void* getMappedPtr() const { return allocation.mappedPtr; }
    WGPUBuffer getBuffer() const { return allocation.buffer; }
    u32 getOffset() const { return allocation.offset; }
    u32 getSize() const { return allocation.size; }

    // Manual release
    void release() {
        if (!released && pool) {
            pool->deallocateMemory(allocation);
            released = true;
        }
    }

private:
    CWebGPUMemoryPool* pool;
    MemoryAllocation allocation;
    bool released;
};

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_

#endif // IRR_C_WEBGPU_MEMORY_POOL_H_INCLUDED