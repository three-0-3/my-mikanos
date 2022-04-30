#pragma once

#include <cstdint>

enum SegmentDescriptorType {
	kReadWrite = 2,
	kExecuteRead = 10,
};

union SegmentDescriptor {
  uint64_t data;
  struct {
		uint64_t limit_low : 16;
		uint64_t base_low : 16;
		uint64_t base_middle : 8;
		SegmentDescriptorType type : 4;
		uint64_t system_segment : 1;
		uint64_t descriptor_privilege_level : 2;
		uint64_t present : 1;
		uint64_t limit_high : 4;
		uint64_t available : 1;
		uint64_t long_mode : 1;
		uint64_t default_operation_size : 1;
		uint64_t granulariy : 1;
		uint64_t base_high : 8;
	} __attribute__((packed)) bits;
} __attribute__((packed));

void SetCodeSegment(SegmentDescriptor& desc,
										SegmentDescriptorType type,
										unsigned int descriptor_privilege_level);

void SetDataSegment(SegmentDescriptor& desc,
										SegmentDescriptorType type,
										unsigned int descriptor_privilege_level);

// set segment registers
const uint16_t kKernelCS = 1 << 3;
const uint16_t kKernelSS = 2 << 3;
const uint16_t kKernelDS = 0;

// create and load gdt
void SetupSegments();
void InitializeSegmentation();