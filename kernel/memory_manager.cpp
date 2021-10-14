#include "memory_manager.hpp"

#include "error.hpp"

BitmapMemoryManager::BitmapMemoryManager()
 : alloc_map_{}, range_begin_{FrameID{0}}, range_end_{FrameID{kFrameCount}} {}

WithError<FrameID> BitmapMemoryManager::Allocate(size_t num_frames) {
	size_t start_frame_id = range_begin_.ID();
	// first fit algorithm to search for the available memory
	// worst: O(n)?
	while(true) {
		size_t i = 0;
		for(; i < num_frames; ++i) {
			// reach the end and no memory with enough length found
			if (start_frame_id + i >= range_end_.ID()) {
				return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
			}
			if (GetBit(FrameID{start_frame_id + i})) {
				// break and start again if the current FrameID is already taken
				break;
			}
		}
		if (i == num_frames) {
			// found the memory with enough length (num_frames)
			MarkAllocated(FrameID{start_frame_id}, num_frames);
			return {
				FrameID{start_frame_id},
				MAKE_ERROR(Error::kSuccess),
			};
		}
		// start again from the next frame
		start_frame_id += i + 1;
	}
}

// requires frames length to release, not only the pointer
Error BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames) {
	for (size_t i = 0; i < num_frames; ++i) {
		SetBit(FrameID{start_frame.ID() + i}, false);
	}
	return MAKE_ERROR(Error::kSuccess);
}

void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames) {
	for (size_t i = 0; i < num_frames; ++i) {
		SetBit(FrameID{start_frame.ID() + i}, true);
	}
}

void BitmapMemoryManager::SetMemoryRange(FrameID range_begin, FrameID range_end) {
	range_begin_ = range_begin;
	range_end_ = range_end;
} 

// return true: allocated, false: available
bool BitmapMemoryManager::GetBit(FrameID frame) const {
	auto line_index = frame.ID() / kBitsPerMapLine;
	auto bit_index = frame.ID() % kBitsPerMapLine;

	return (alloc_map_[line_index] & (static_cast<MapLineType>(1) << bit_index)) != 0;
}

void BitmapMemoryManager::SetBit(FrameID frame, bool allocated) {
	auto line_index = frame.ID() / kBitsPerMapLine;
	auto bit_index = frame.ID() % kBitsPerMapLine;

	if (allocated) {
		// to mark allocated, get OR
		alloc_map_[line_index] |= (static_cast<MapLineType>(1) << bit_index);
	} else {
		// to mark available, get AND with inverted bit string
		alloc_map_[line_index] &= ~(static_cast<MapLineType>(1) << bit_index);
	}
}