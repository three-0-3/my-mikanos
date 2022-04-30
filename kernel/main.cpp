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
    if (layer && layer->IsDraggable()) {
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

  // set mouse callback method
  usb::HIDMouseDriver::default_observer = MouseObserver;

  // create new window for background (no draw yet)
  screen_size.x = screen_config.horizontal_resolution;
  screen_size.y = screen_config.vertical_resolution;

  auto bgwindow = std::make_shared<Window>(screen_size.x, screen_size.y, screen_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  // save background design data to bgwindow
  DrawDesktop(*bgwriter);

  // create new window for mouse cursor
  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  // save mouse cursor desgin data to mouse_window
  DrawMouseCursor(mouse_window->Writer(), {0, 0});
  mouse_position = {200, 200};

  auto main_window = std::make_shared<Window>(160, 52, screen_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hellow Window");

  auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
  console->SetWindow(console_window);

  FrameBuffer screen;
  if (auto err = screen.Initialize(screen_config)) {
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
    .SetDraggable(true)
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
