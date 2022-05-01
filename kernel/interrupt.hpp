#pragma once

#include <array>
#include <cstdint>
#include <deque>

#include "message.hpp"

enum DescriptorType {
	kInterruptGate = 14,
};

union InterruptDescriptorAttribute {
	uint16_t data;
	struct {
		uint16_t interrupt_stack_table : 3;
		uint16_t : 5;
		DescriptorType type : 4;
		uint16_t : 1;
		uint16_t descriptor_privilage_level : 2;
		uint16_t present : 1;
	} __attribute__((packed)) bits;
} __attribute__((packed));

struct InterruptDescriptor {
	uint16_t offset_low;
	uint16_t segment_selector;
	InterruptDescriptorAttribute attr;
	uint16_t offset_middle;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed));

extern std::array<InterruptDescriptor, 256> idt;

constexpr InterruptDescriptorAttribute MakeIDTAttr(
		DescriptorType type,
		uint8_t descriptor_privilege_level,
		bool present = true,
		uint8_t interrupt_stack_table = 0) {
	InterruptDescriptorAttribute attr{};
	attr.bits.interrupt_stack_table = interrupt_stack_table;
	attr.bits.type = type;
	attr.bits.descriptor_privilage_level = descriptor_privilege_level;
	attr.bits.present = present;
	return attr;
}

// set the value of attribute, offset, segment selector to the interrupt descriptor entry
void SetIDTEntry(InterruptDescriptor& desc,
								 InterruptDescriptorAttribute attr,
								 uint64_t offset,
								 uint16_t segment_selector);

enum InterruptVector {
	kXHCI = 0x40,
	kLAPICTimer = 0x41,
};

// interrupt handler receives the following data when it's called
struct InterruptFrame {
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

// Notify End of Interrupt at the end of the interrupt handler
void NotifyEndOfInterrupt();

// void InitializeInterrupt(ArrayQueue<Message>* msg_queue);
void InitializeInterrupt(std::deque<Message>* msg_queue);