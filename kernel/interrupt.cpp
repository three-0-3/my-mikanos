#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"
#include "task.hpp"

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

namespace {
	// interrupt handler function for USB(XHC)
	__attribute__((interrupt))
	void IntHandlerXHCI(InterruptFrame* frame) {
		task_manager->SendMessage(1, Message{Message::kInterruptXHCI}); // 1 is main task
		NotifyEndOfInterrupt();
	}

	// interrupt handler function for Local APIC Timer
	__attribute__((interrupt))
	void IntHandlerLAPICTimer(InterruptFrame* frame) {
		LAPICTimerOnInterrupt();
	}
}

void InitializeInterrupt() {	
	// Set Interrupt Descriptor Table and load to CPU
  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), kKernelCS);
	SetIDTEntry(idt[InterruptVector::kLAPICTimer], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
							reinterpret_cast<uint64_t>(IntHandlerLAPICTimer), kKernelCS);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}