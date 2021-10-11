#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
	// CONFIG_ADDRESS register's IO port address (constant)
	const uint16_t kConfigAddress = 0x0cf8;
	// CONFIG_DATA register's IO port address (constant)
	const uint16_t kConfigData = 0x0cfc;

	// PCI device class code
	struct ClassCode {
		uint8_t base, sub, interface;

		// return true if the base class is same
		bool Match(uint8_t b) { return b == base; }
		// return true if base and sub class are same
		bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
		// return true if base and sub class and interface are same
		bool Match(uint8_t b, uint8_t s, uint8_t i) {
			return Match(b, s) && i == interface;
		}
	};

	// Device basic data
	// bus, device, function identify a deivce
	struct Device {
		uint8_t bus, device, function, header_type;
		ClassCode class_code;
	};

	// write the address to CONFIG_ADDRESS register
	void WriteAddress(uint32_t address);
	// write the value to CONFIG_DATA register
	void WriteData(uint32_t value);
	// read the value from CONFIG_DATA register
	uint32_t ReadData();

	// Read the vendor id
	uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
	// Read the device id
	uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
	// Read the header type
	uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
	// Read the class code
	ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

	inline uint16_t ReadVendorId(const Device& dev) {
		return ReadVendorId(dev.bus, dev.device, dev.function);
	}

	// Read bus number register
	uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);
	// Check whether it is single function device
	bool IsSingleFunctionDevice(uint8_t header_type);

	// the list of the PCI devices found by ScanAllBus()
	inline std::array<Device, 32> devices;
	// the number of the active devices
	inline int num_device;

	// Scan all the PCI devices recursively from bus 0 and save to devices
	Error ScanAllBus();

	constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
		return 0x10 + 4 * bar_index;
	}

	WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);

	union CapabilityHeader {
		uint32_t data;
		struct {
			uint32_t cap_id : 8;
			uint32_t next_ptr : 8;
			uint32_t cap : 16;
		} __attribute__((packed)) bits;
	} __attribute__((packed));

	const uint8_t kCapabilityMSI = 0x05;
	const uint8_t kCapabilityMSIX = 0x11;

	CapabilityHeader ReadCapabilityHeader(const Device& dev, uint8_t addr);

	struct MSICapability {
		union {
			uint32_t data;
			struct {
				uint32_t cap_id : 8;
				uint32_t next_ptr : 8;
				uint32_t msi_enable : 1;
				uint32_t multi_msg_capable : 3;
				uint32_t multi_msg_enable : 3;
				uint32_t addr_64_capable : 1;
				uint32_t per_vector_mask_capable : 1;
				uint32_t : 7;
			} __attribute__((packed)) bits;
		} __attribute__((packed)) header;

		uint32_t msg_addr;
		uint32_t msg_upper_addr;
		uint32_t msg_data;
		uint32_t mask_bits;
		uint32_t pending_bits;
	} __attribute__((packed));

	enum class MSITriggerMode {
		kLevel = 1,
	};

	enum MSIDeliveryMode {
		kFixed = 0b000,
	};

	Error ConfigureMSIFixedDestination(
		const Device&dev, uint8_t apic_id,
		MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
		uint8_t vector, unsigned int num_vector_exponent);
}