#include "pci.hpp"
#include "asmfunc.h"

namespace pci {
	// make 32 bit integer for CONFIG_ADDRESS register
	uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_addr) {
		auto shl = [](uint32_t x, unsigned int bits) {
			return x << bits;
		};

		return shl(1, 31) // enable bit
				| shl(bus, 16)
				| shl(device, 11)
				| shl(function, 8)
				| (reg_addr & 0xfcu); // offset by 4bytes
	}

	// Add bus/device/function/header type to the list
	Error AddDevice(uint8_t bus, uint8_t device, uint8_t function, uint8_t header_type) {
		if (num_device == devices.size()) {
			return MAKE_ERROR(Error::kFull);
		}

		devices[num_device] = Device{bus, device, function, header_type};
		++num_device;
		return MAKE_ERROR(Error::kSuccess);
	}

	Error ScanBus(uint8_t bus);

	// Add device(function) info to the list
	// Scan PCI-to-PCI bridge recursively
	Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
		// Get header_type of this function
		auto header_type = ReadHeaderType(bus, device, function);
		// Add the device of this function to the array
		if (auto err = AddDevice(bus, device, function, header_type)) {
			return err;
		}

		// Check the class code and search for the devices recursively if it's PCI-to-PCI Bridge
		// Parse class code from the header type
		auto class_code = ReadClassCode(bus, device, function);
		uint8_t base = (class_code >> 24) & 0xffu;
		uint8_t sub = (class_code >> 16) & 0xffu;
		// base class 0x06 - Bridge
		// sub  class 0x04 - PCI-to-PCI Bridge
		if (base == 0x06u && sub == 0x04u) {
			auto bus_numbers = ReadBusNumbers(bus, device, function);
			uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
			return pci::ScanBus(secondary_bus);
		}

		return MAKE_ERROR(Error::kSuccess);
	}

	// Scan the device of the specified number
	// Run ScanFunction if the valid function is found
	Error ScanDevice(uint8_t bus, uint8_t device) {
		// Scan function 0 of this device at first
		if (auto err = ScanFunction(bus, device, 0)) {
			return err;
		}
		// If it is single function device, finish
		if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
			return MAKE_ERROR(Error::kSuccess);
		}

		// If it is multi function device, scan for the other functions
		for (uint8_t function = 1; function < 8; ++function) {
			if (ReadVendorId(bus, device, function) == 0xffffu) {
				continue;
			}
			if (auto err = ScanFunction(bus, device, function)) {
				return err;
			}
		}
		return MAKE_ERROR(Error::kSuccess);
	}
	
	// Scan bus of the specified number for PCI devices
	// Run ScanDevice if the valid device is found
	Error ScanBus(uint8_t bus) {
		for (uint8_t device = 0; device < 32; ++device) {
			if (ReadVendorId(bus, device, 0) == 0xffffu) {
				continue;
			}
			if (auto err = ScanDevice(bus, device)) {
				return err;
			}
		}
		return MAKE_ERROR(Error::kSuccess);
	}

	// Write the address to CONFIG_ADDRESS register
	void WriteAddress(uint32_t address) {
		IoOut32(kConfigAddress, address);
	}

	// Read the value from CONFIG_DATA register
	uint32_t ReadData() {
		return IoIn32(kConfigData);
	}

	// Read the vendor id
	uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function) {
		WriteAddress(MakeAddress(bus, device, function, 0x00));
		return ReadData();
	}

	// Read the device id
	uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function) {
		WriteAddress(MakeAddress(bus, device, function, 0x00));
		return ReadData() >> 16;
	}

	// Read the header type
	uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
		WriteAddress(MakeAddress(bus, device, function, 0x0c));
		return ReadData() >> 16;
	}

  // Read the class code
	uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
		WriteAddress(MakeAddress(bus, device, function, 0x08));
		return ReadData();
	}

	// Read bus number register
	uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function) {
		WriteAddress(MakeAddress(bus, device, function, 0x18));
		return ReadData();
	}

	// Check whether it is single function device
	bool IsSingleFunctionDevice(uint8_t header_type) {
		return (header_type & 0x80u) == 0;
	}

	// Scan all the PCI devices recursively from bus 0 and save to devices
	Error ScanAllBus() {
		num_device = 0;

		// Read the header type of the PCI Host Bridge
		auto header_type = ReadHeaderType(0, 0, 0);
		// If PCI Host Bridge is the single function, scan bus 0 and finish
		if (IsSingleFunctionDevice(header_type)) {
			return ScanBus(0);
		}
		// If PCI Host Bridge is the multi function, scan each active bus
		for (uint8_t function = 1; function < 8; ++function) {
			// Vendor ID is 0xffffu for the inactive bus
			if (ReadVendorId(0, 0, function) == 0xffffu) {
				continue;
			}
			// Scan bus
			if (auto err = ScanBus(function)) {
				return err;
			}
		}
		return MAKE_ERROR(Error::kSuccess);
	}
}