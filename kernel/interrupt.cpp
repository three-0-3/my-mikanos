#include "interrupt.hpp"

std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(InterruptDescriptor& desc,
								 InterruptDescriptorAttribute attr,
								 uint64_t offset,
								 uint16_t segment_selector) {
  desc.attr = attr;
	//16bit at the bottom
	desc.offset_low = offset & 0xffffu;
	//16bit in the middle
	desc.offset_middle = (offset >> 16) & 0xffffu;
	//32bit at the top
  desc.offset_high = offset >> 32;
	desc.segment_selector = segment_selector;
}

void NotifyEndOfInterrupt() {
	// 0xfee00000 ~ 0xfee0400 (1024byte) in the memory space is the registers, not the actual memory
	// write something to 0xfee000b0 means "end of interrupt"
	// use volatile not to optimize
	volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
	*end_of_interrupt = 0;
}