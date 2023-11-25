#pragma once

#include <cstddef>
#include <limits>

#include "error.hpp"
#include "memory_map.hpp"

// user-defined literals for KiB/MiB/GiB
namespace {
  constexpr unsigned long long operator""_KiB(unsigned long long kib) {
    return kib * 1024;
  }

  constexpr unsigned long long operator""_MiB(unsigned long long mib) {
    return mib * 1024_KiB;
  }

  constexpr unsigned long long operator""_GiB(unsigned long long gib) {
    return gib * 1024_MiB;
  }
}

// size of a physical memory frame
static const auto kBytesPerFrame{4_KiB};

// wrapper class for size_t variables id for the clarity
class FrameID {
  public:
    // constructor (explicit: cannot inintialize by substitution)
    explicit FrameID(size_t id) : id_{id} {}
    // return id
    size_t ID() const { return id_; }
    // return pointer of the frame
    void* Frame() const { return reinterpret_cast<void*>(id_ * kBytesPerFrame); }

  private:
    size_t id_;
};

// null frame id to be returned when there is not enough memory
static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

struct MemoryStat {
  size_t allocated_frames;
  size_t total_frames;
};

class BitmapMemoryManager {
  public:
    // constructor
    BitmapMemoryManager();

    // allocate and free
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);
    // mark allocated
    void MarkAllocated(FrameID start_frame, size_t num_frames);

    // set the range of the memory manager
    void SetMemoryRange(FrameID range_begin, FrameID range_end);

    MemoryStat Stat() const;

  private:
    // max physical memory of this memory manager
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    // number of frames to keep kMaxPhysicalMemoryBytes
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

    // a line in the memory bitmap (32 bit per line)
    using MapLineType = unsigned long;
    // number of the frames to keep in a line of the bitmap
    // (not that all the bits in MapLineType are used)
    static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};

    // bitmap to keep the memory allocation status
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;
    // start point of the memory range under this memory manager
    FrameID range_begin_;
    // end point of the memory range under this memory manager
    FrameID range_end_;

    bool GetBit(FrameID frame) const;
    void SetBit(FrameID frame, bool allocated);
};

extern BitmapMemoryManager* memory_manager;
void InitializeMemoryManager(const MemoryMap& memory_map);