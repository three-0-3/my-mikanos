#include <cstdio>

#include <deque>

#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "interrupt.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "layer.hpp"
#include "timer.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"

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

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());

}

std::shared_ptr<Window> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow() {
  const int win_w = 160;
  const int win_h = 52;

  text_window = std::make_shared<Window>(win_w, win_h, screen_config.pixel_format);
  DrawWindow(*text_window->Writer(), "Text Box Test");
  DrawTextbox(*text_window->Writer(), {4, 24}, {win_w - 8, win_h - 24 - 4});

  text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .Move({350, 200})
    .ID();

  layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

int text_window_index;

void DrawTextCursor(bool visible) {
  const auto color = visible? ToColor(0) : ToColor(0xffffff);
  const auto pos = Vector2D<int>{8 + 8 * text_window_index, 24 + 5};
  FillRectangle(*text_window->Writer(), pos, {7, 15}, color);
}

void InputTextWindow(char c) {
  if (c == 0) {
    return;
  }

  auto pos = []() { return Vector2D<int>{8 + 8 * text_window_index, 24 + 6}; };

  const int max_chars = (text_window->Width() - 16) / 8 - 1;
  if (c == '\b' && text_window_index > 0) {
    DrawTextCursor(false);
    --text_window_index;
    FillRectangle(*text_window->Writer(), pos(), {8, 16}, ToColor(0xffffff));
    DrawTextCursor(true);
  } else if (c >= ' ' && text_window_index < max_chars) {
    DrawTextCursor(false);
    WriteAscii(*text_window->Writer(), pos(), c, ToColor(0));
    ++text_window_index;
    DrawTextCursor(true);
  }

  layer_manager->Draw(text_window_layer_id);
}

// Main stack (not created by UEFI)
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table) {
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
  std::deque<Message> main_queue{32};
  InitializeInterrupt(&main_queue);

  InitializePCI();
  usb::xhci::Initialize();

  InitializeLayer();
  InitializeMainWindow();
  InitializeTextWindow();
  InitializeMouse();

  // draw all the layers
  layer_manager->Draw({{0, 0}, ScreenSize()});

  acpi::Initialize(acpi_table); // validate RSDP
  InitializeLAPICTimer(main_queue);

  InitializeKeyboard(main_queue);

  const int kTextboxCursorTimer = 1;
  const int kTimer05sec = static_cast<int>(kTimerFreq * 0.5);
  __asm__("cli");
  timer_manager->AddTimer(Timer{kTimer05sec, kTextboxCursorTimer});
  __asm__("sti");
  bool textbox_cursor_visible = false;

  // counter to show on the main window
  char str[128];

  while (true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    // counter to show on the main window
    sprintf(str, "%010lu", tick);
    // cleanup the previous number
    FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    // display the counter
    WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // disable interrupt
    __asm__("cli");
    // if there is no message in the queue, enable interrupt and halt
    if (main_queue.size() == 0) {
      __asm__("sti\n\thlt");
      continue;
    }

    // get one message
    Message msg = main_queue.front();
    // delete one message
    main_queue.pop_front();
    // enable interrupt
    __asm__("sti");

    switch (msg.type) {
    case Message::kInterruptXHCI:
      usb::xhci::ProcessEvents();
      break;
    case Message::kTimerTimeout:
      if (msg.arg.timer.value == kTextboxCursorTimer) {
        __asm__("cli");
        timer_manager->AddTimer(Timer{msg.arg.timer.timeout + kTimer05sec, kTextboxCursorTimer});
        __asm__("sti");
        textbox_cursor_visible = !textbox_cursor_visible;
        DrawTextCursor(textbox_cursor_visible);
        layer_manager->Draw(text_window_layer_id);
      }
      break;
    case Message::kKeyPush:
      InputTextWindow(msg.arg.keyboard.ascii);
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
