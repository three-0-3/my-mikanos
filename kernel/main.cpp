#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "memory_map.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/classdriver/mouse.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"

void operator delete(void* obj) noexcept {
}

// colors for desktop
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

// writer
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

// console
char console_buf[sizeof(Console)];
Console* console;

// print string (to delete?)
int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

// mouse cursor drawer
char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

usb::xhci::Controller* xhc;

struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};

ArrayQueue<Message>* main_queue;

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
  main_queue->Push(Message{Message::kInterruptXHCI});
  NotifyEndOfInterrupt();
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config,
                           const MemoryMap& memory_map) {
  // set pixel writer according to the frame buffer config
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }

  // draw desktop
  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;

  // background
  FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
  // menu bar
  FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
  // start button (right in menu bar)
  FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
  DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {kFrameWidth / 5 - 20, 30}, {160, 160, 160});

  // create object for mouse cursor
  mouse_cursor = new(mouse_cursor_buf) MouseCursor{
    pixel_writer, kDesktopBGColor, {400, 200}
  };
  // mouse cursor move test
  mouse_cursor->MoveRelative({0, 200});

  // Write welcome message in the console
  console = new(console_buf) Console{*pixel_writer, kDesktopFGColor, kDesktopBGColor};
  printk("Welcome to MikanOS!!\n");
  {
    LogLevel log_level = kWarn;
    SetLogLevel(log_level);
    Log(kInfo, "Log Level: %d\n", log_level);
  }

  // memory types which is allocatable for application
  const std::array available_memory_types{
    MemoryType::kEfiBootServicesCode,
    MemoryType::kEfiBootServicesData,
    MemoryType::kEfiConventionalMemory,
  };

  // print memory map (only allocatables)
  printk("memory_map: %p\n", &memory_map);
  for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.buffer);
       iter < reinterpret_cast<uintptr_t>(memory_map.buffer) + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
    for (int i = 0; i < available_memory_types.size(); ++i) {
      if (desc->type == available_memory_types[i]) {
        printk("type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
               desc->type,
               desc->physical_start,
               desc->physical_start + desc->number_of_pages * 4096 - 1,
               desc->number_of_pages,
               desc->attribute);
      }
    }
  }

  std::array<Message, 32> main_queue_data;
  ArrayQueue<Message> main_queue{main_queue_data};
  ::main_queue = &main_queue;

  // Run function to scan all the devices in PCI space and save it to the global variable
  auto err = pci::ScanAllBus();
  Log(kDebug, "ScanAllBus: %s\n", err.Name());

  // Go through the devices list and print with vendor id & class code
  for (int i = 0; i < pci::num_device; ++i) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
    auto device_id = pci::ReadDeviceId(dev.bus, dev.device, dev.function);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    Log(kDebug, "%d.%d.%d: vend %04x, device, %04x, class %08x, head %02x\n",
            dev.bus, dev.device, dev.function,
            vendor_id, device_id, class_code, dev.header_type);
  }

  // search for xHC (priority: intel)
  pci::Device* xhc_dev = nullptr;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
      xhc_dev = &pci::devices[i];

      // exit loop when the intel xHC is found
      if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        break;
      }
    }
  }

  if (xhc_dev) {
    Log(kInfo, "xHC has been found: %d.%d.%d\n",
        xhc_dev->bus, xhc_dev->device, xhc_dev->function);
  } else {
    Log(kError, "xHC has not been found\n");
  }

  // Set Interrupt Descriptor Table and load to CPU
  const uint16_t cs = GetCS();
  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), cs);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

	// 0xfee00000 ~ 0xfee0400 (1024byte) in the memory space is the registers, not the actual memory
	// 31:24 of 0xfee00020 is "local APIC ID" of the running CPU core
  const uint8_t bsp_local_apic_id = *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
  pci::ConfigureMSIFixedDestination(
    *xhc_dev,
    bsp_local_apic_id,
    pci::MSITriggerMode::kLevel,
    pci::MSIDeliveryMode::kFixed,
    InterruptVector::kXHCI,
    0
  );

  // read BAR0 of xHC PIC config space 
  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
  // get MMIO base address
  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

  // declare xhc
  usb::xhci::Controller xhc{xhc_mmio_base};

  {
    // initialize xhc
    auto err = xhc.Initialize();
    Log(kDebug, "xhc.Initialize: %s\n", err.Name());
  }

  Log(kInfo, "xHC starting\n");
  xhc.Run();

  // substitute block scope xhc to file scope xhc
  ::xhc = &xhc;
  // sti instruction set the system flag "Interrupt Flag"
  // to be ready for maskable hardware interrupts
  __asm__("sti");

  // set mouse callback method
  usb::HIDMouseDriver::default_observer = MouseObserver;

  // configure port (necessary for QEMU)
  for (int i = 1; i <= xhc.MaxPorts(); ++i) {
    auto port = xhc.PortAt(i);
    Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

    if (port.IsConnected()) {
      if (auto err = ConfigurePort(xhc, port)) {
        Log(kError, "failed to configure port: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        continue;
      }
    }
  }

  while (true) {
    // disable interrupt
    __asm__("cli");
    // if there is no message in the queue, enable interrupt and halt
    if (main_queue.Count() == 0) {
      __asm__("sti\n\thlt");
      continue;
    }

    // get one message
    Message msg = main_queue.Front();
    // delete one message
    main_queue.Pop();
    // enable interrupt
    __asm__("sti");

    switch (msg.type) {
    case Message::kInterruptXHCI:
      while (xhc.PrimaryEventRing()->HasFront()) {
        if (auto err = ProcessEvent(xhc)) {
          Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
              err.Name(), err.File(), err.Line());
        }
      }
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
