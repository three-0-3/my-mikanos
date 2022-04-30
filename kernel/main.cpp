#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <deque>

#include "memory_map.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "mouse.hpp"
#include "usb/xhci/xhci.hpp"
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

std::shared_ptr<Window> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
  main_window = std::make_shared<Window>(160, 52, screen_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hellow Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 300})
    .ID();

  layer_manager->UpDown(main_window_layer_id, 2);

}

// Main stack (not created by UEFI)
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref) {
  // copy the data from UEFI to the local variables (not to be overwritten)
  MemoryMap memory_map{memory_map_ref};

  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();

  printk("Welcome to MikanOS!!\n");
  {
    LogLevel log_level = kWarn;
    SetLogLevel(log_level);
    Log(kInfo, "Log Level: %d\n", log_level);
  }

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  std::array<Message, 32> main_queue_data;
  ArrayQueue<Message> main_queue{main_queue_data};
  InitializeInterrupt(&main_queue);

  InitializePCI();
  usb::xhci::Initialize();

  InitializeLayer();
  InitializeMainWindow();
  InitializeMouse();

  // draw all the layers
  layer_manager->Draw({{0, 0}, ScreenSize()});

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
      usb::xhci::ProcessEvents();
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
