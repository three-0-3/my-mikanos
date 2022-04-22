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
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "layer.hpp"
#include "window.hpp"

void operator delete(void* obj) noexcept {
}

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

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

// mouse layer id defined here to be used in MouseObserver
unsigned int mouse_layer_id;
Vector2D<int> screen_size;
Vector2D<int> mouse_position;

void MouseObserver(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  static unsigned int mouse_drag_layer_id = 0;
  static uint8_t previous_buttons = 0;

  const auto oldpos = mouse_position;
  // move the mouse cursor window
  auto newpos = mouse_position + Vector2D<int>{displacement_x, displacement_y};
  // limit mouse cursor move inside the screen area
  mouse_position = ElementMax(ElementMin(newpos, screen_size + Vector2D<int>{-1, -1}), {0, 0});

  const auto posdiff = mouse_position - oldpos;

  layer_manager->Move(mouse_layer_id, mouse_position);

  const bool previous_left_pressed = (previous_buttons & 0x01);
  const bool left_pressed = (buttons & 0x01);
  if (!previous_left_pressed && left_pressed) {
    auto layer = layer_manager->FindLayerByPosition(mouse_position, mouse_layer_id);
    if (layer) {
      mouse_drag_layer_id = layer->ID();
    }
  } else if (previous_left_pressed && left_pressed) {
    if (mouse_drag_layer_id > 0) {
      layer_manager->MoveRelative(mouse_drag_layer_id, posdiff);
    }
  } else if (previous_left_pressed && !left_pressed) {
    mouse_drag_layer_id = 0;
  }

  previous_buttons = buttons;
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

// Main stack (not created by UEFI)
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref) {
  // copy the data from UEFI to the local variables (not to be overwritten)
  FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
  MemoryMap memory_map{memory_map_ref};

  // set pixel writer according to the frame buffer config
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter(frame_buffer_config);
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }

  // Write welcome message in the console
  console = new(console_buf) Console{kDesktopFGColor, kDesktopBGColor};
  console->SetWriter(pixel_writer);
  printk("Welcome to MikanOS!!\n");
  {
    LogLevel log_level = kWarn;
    SetLogLevel(log_level);
    Log(kInfo, "Log Level: %d\n", log_level);
  }

  // create and load gdt
  SetupSegments();
  // set segment registers
  const uint16_t kernel_cs = 1 << 3;
  const uint16_t kernel_ss = 2 << 3;
  // set null descriptor for unused registers
  SetDSAll(0);
  // set code and data segment descriptor to CS/SS registers
  SetCSSS(kernel_cs, kernel_ss);

  // create and set page table (hierarchical paging structure)
  SetupIdentityPageTable();

  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  // end address of the last avaialble memory descriptor
  uintptr_t available_end = 0;
  // initialize memory manager by checking UEFI memory map
  for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.buffer);
       iter < reinterpret_cast<uintptr_t>(memory_map.buffer) + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
    // mark allocated if there is any gap between the last checked end address and the current desc start address
    if (available_end < desc->physical_start) {
      memory_manager->MarkAllocated(
        FrameID{available_end / kBytesPerFrame},
        (desc->physical_start - available_end) / kBytesPerFrame);
    }

    const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      // if the current desc is available, extend available_end
      available_end = physical_end;
    } else {
      // if not, mark allocated
      memory_manager->MarkAllocated(
        FrameID{desc->physical_start / kBytesPerFrame},
        desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }

  // set the range of the memory manager using the result of the memory map check
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

  // initialize the value for heap allocation (sbrk in newlib_support.c)
  if (auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
    exit(1);
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

  // create new window for background (no draw yet)
  screen_size.x = frame_buffer_config.horizontal_resolution;
  screen_size.y = frame_buffer_config.vertical_resolution;

  auto bgwindow = std::make_shared<Window>(screen_size.x, screen_size.y, frame_buffer_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  // save background design data to bgwindow
  DrawDesktop(*bgwriter);

  // create new window for mouse cursor
  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  // save mouse cursor desgin data to mouse_window
  DrawMouseCursor(mouse_window->Writer(), {0, 0});
  mouse_position = {200, 200};

  auto main_window = std::make_shared<Window>(160, 52, frame_buffer_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hellow Window");

  auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16, frame_buffer_config.pixel_format);
  console->SetWindow(console_window);

  FrameBuffer screen;
  if (auto err = screen.Initialize(frame_buffer_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
  }

  // create layer manager to control all the layers/windows
  layer_manager = new LayerManager;
  // set pixel writer (not window writer) to layer manager
  layer_manager->SetWriter(&screen);

  // add new layer for background window
  auto bglayer_id = layer_manager->NewLayer()
    .SetWindow(bgwindow)
    .Move({0, 0})
    .ID();
  mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .Move(mouse_position)
    .ID();
  auto main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .Move({300, 300})
    .ID();
  console->SetLayerID(layer_manager->NewLayer()
    .SetWindow(console_window)
    .Move({0,0})
    .ID());

  // set the drawing order of layers
  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(console->LayerID(), 1);
  layer_manager->UpDown(main_window_layer_id, 2);
  layer_manager->UpDown(mouse_layer_id, 3);
  // draw all the layers
  layer_manager->Draw({{0, 0}, screen_size});

  // counter to show on the main window
  char str[128];
  unsigned int count = 0;


  while (true) {
    // counter to show on the main window
    ++count;
    sprintf(str, "%010u", count);
    // cleanup the previous number
    FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    // display the counter
    WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // disable interrupt
    __asm__("cli");
    // if there is no message in the queue, enable interrupt and halt
    if (main_queue.Count() == 0) {
      __asm__("sti\n");
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
