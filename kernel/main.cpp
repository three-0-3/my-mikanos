#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "mouse.hpp"

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

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
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
    LogLevel log_level = kDebug;
    SetLogLevel(log_level);
    Log(kInfo, "Log Level: %d\n", log_level);
  }

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

  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

  while (1) __asm__("hlt");
}
