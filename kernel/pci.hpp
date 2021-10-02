#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
	// CONFIG_ADDRESS register's IO port address (constant)
	const uint16_t kConfigAddress = 0x0cf8;
	// CONFIG_DATA register's IO port address (constant)
	const uint16_t kConfigData = 0x0cfc;

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
	uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

	// Read bus number register
	uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);
	// Check whether it is single function device
	bool IsSingleFunctionDevice(uint8_t header_type);

	// Device basic data
	// bus, device, function identify a deivce
	struct Device {
		uint8_t bus, device, function, header_type;
	};

	// the list of the PCI devices found by ScanAllBus()
	inline std::array<Device, 32> devices;
	// the number of the active devices
	inline int num_device;

	// Scan all the PCI devices recursively from bus 0 and save to devices
	Error ScanAllBus();
}