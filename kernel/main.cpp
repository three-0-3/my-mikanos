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
#include "asmfunc.h"
#include "task.hpp"
#include "terminal.hpp"
#include "fat.hpp"
#include "syscall.hpp"

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

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
  main_window = std::make_shared<ToplevelWindow>(160, 52, screen_config.pixel_format, "Hellow Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());

}

std::shared_ptr<ToplevelWindow> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow() {
  const int win_w = 160;
  const int win_h = 52;

  text_window = std::make_shared<ToplevelWindow>(win_w, win_h, screen_config.pixel_format, "Text Box Test");
  DrawTextbox(*text_window->InnerWriter(), {0, 0}, text_window->InnerSize());

  text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .Move({500, 100})
    .ID();

  layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

int text_window_index;

void DrawTextCursor(bool visible) {
  const auto color = visible? ToColor(0) : ToColor(0xffffff);
  const auto pos = Vector2D<int>{4 + 8 * text_window_index, 5};
  FillRectangle(*text_window->InnerWriter(), pos, {7, 15}, color);
}

void InputTextWindow(char c) {
  if (c == 0) {
    return;
  }

  auto pos = []() { return Vector2D<int>{4 + 8 * text_window_index, 6}; };

  const int max_chars = (text_window->InnerSize().x - 8) / 8 - 1;
  if (c == '\b' && text_window_index > 0) {
    DrawTextCursor(false);
    --text_window_index;
    FillRectangle(*text_window->InnerWriter(), pos(), {8, 16}, ToColor(0xffffff));
    DrawTextCursor(true);
  } else if (c >= ' ' && text_window_index < max_chars) {
    DrawTextCursor(false);
    WriteAscii(*text_window->InnerWriter(), pos(), c, ToColor(0));
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
    const acpi::RSDP& acpi_table,
    void* volume_image) {
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
  InitializeTSS();
  InitializeInterrupt();

  fat::Initialize(volume_image);
  InitializePCI();

  InitializeLayer();
  InitializeMainWindow();
  InitializeTextWindow();

  // draw all the layers
  layer_manager->Draw({{0, 0}, ScreenSize()});

  acpi::Initialize(acpi_table); // validate RSDP
  InitializeLAPICTimer();

  const int kTextboxCursorTimer = 1;
  const int kTimer05sec = static_cast<int>(kTimerFreq * 0.5);
  timer_manager->AddTimer(Timer{kTimer05sec, kTextboxCursorTimer});
  bool textbox_cursor_visible = false;

  InitializeSyscall();

  InitializeTask();
  Task& main_task = task_manager->CurrentTask();
  const uint64_t task_terminal_id = task_manager->NewTask()
    .InitContext(TaskTerminal, 0)
    .Wakeup()
    .ID();

  usb::xhci::Initialize();
  InitializeKeyboard();
  InitializeMouse();

  // counter to show on the main window
  char str[128];

  while (true) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    // counter to show on the main window
    sprintf(str, "%010lu", tick);
    // cleanup the previous number
    FillRectangle(*main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    // display the counter
    WriteString(*main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // disable interrupt
    __asm__("cli");
    // if there is no message in the queue, enable interrupt and halt
    auto msg = main_task.ReceiveMessage();
    if (!msg) {
      main_task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    switch (msg->type) {
    case Message::kInterruptXHCI:
      usb::xhci::ProcessEvents();
      break;
    case Message::kTimerTimeout:
      if (msg->arg.timer.value == kTextboxCursorTimer) {
        __asm__("cli");
        timer_manager->AddTimer(Timer{msg->arg.timer.timeout + kTimer05sec, kTextboxCursorTimer});
        __asm__("sti");
        textbox_cursor_visible = !textbox_cursor_visible;
        DrawTextCursor(textbox_cursor_visible);
        layer_manager->Draw(text_window_layer_id);
        __asm__("cli");
        task_manager->SendMessage(task_terminal_id, *msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      if (auto act = active_layer->GetActive(); act == text_window_layer_id) {
        InputTextWindow(msg->arg.keyboard.ascii);
      } else {
        __asm__("cli");
        auto task_it = layer_task_map->find(act);
        __asm__("sti");
        if (task_it != layer_task_map->end()) {
          __asm__("cli");
          task_manager->SendMessage(task_it->second, *msg);
          __asm__("sti");
        } else {
          printk("key push not handled: keycode %02x, ascii %02x\n",
              msg->arg.keyboard.keycode,
              msg->arg.keyboard.ascii);
        }
      }
      break;
    case Message::kLayer:
      ProcessLayerMessage(*msg);
      __asm__("cli");
      task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
      __asm__("sti");
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg->type);
    }
  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
