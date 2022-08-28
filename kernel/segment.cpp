#include "segment.hpp"

#include <array>

#include "asmfunc.h"

// gdt is never used outside of this file
namespace {
	std::array<SegmentDescriptor, 5> gdt;
}

// create the code segment descriptor entry to gdt
void SetCodeSegment(SegmentDescriptor& desc,
										SegmentDescriptorType type,
										unsigned int descriptor_privilege_level) {

	// initialize all the bits with 0
	desc.data = 0;

	// no definition for the following items (ignored in 64 bit mode)
	//   base_low
	//   base_middle
	//   base_high
	//   limit_low
	//   limit_high

	desc.bits.type = type;
	desc.bits.system_segment = 1; // 1: code & data segment
	desc.bits.descriptor_privilege_level = descriptor_privilege_level;
	desc.bits.present = 1; // 1: active descriptor
	desc.bits.available = 0; // reserved for free usage, not used
	desc.bits.long_mode = 1; // 1: 64 bit mode
	desc.bits.default_operation_size = 0; // should be 0 when long_mode == 1
	desc.bits.granulariy = 0; // 1: limit interpreted by 4KB (ignored in 64 bit mode)
}

void SetDataSegment(SegmentDescriptor& desc,
										SegmentDescriptorType type,
										unsigned int descriptor_privilege_level) {
  SetCodeSegment(desc, type, descriptor_privilege_level);
	desc.bits.long_mode = 0; // long_mode is the field for code segment
	desc.bits.default_operation_size = 1; // set 1 to be in sync with syscall (to be define later) 	
}

void SetupSegments() {
	gdt[0].data = 0;
	SetCodeSegment(gdt[1], SegmentDescriptorType::kExecuteRead, 0);
	SetDataSegment(gdt[2], SegmentDescriptorType::kReadWrite, 0);
	SetCodeSegment(gdt[3], SegmentDescriptorType::kExecuteRead, 3);
	SetDataSegment(gdt[4], SegmentDescriptorType::kReadWrite, 3);
	LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

void InitializeSegmentation() {
	SetupSegments();

  // set null descriptor for unused registers
	SetDSAll(kKernelDS);
  // set code and data segment descriptor to CS/SS registers
	SetCSSS(kKernelCS, kKernelSS);
}