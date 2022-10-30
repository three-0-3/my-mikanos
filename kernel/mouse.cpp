#include "mouse.hpp"

#include "graphics.hpp"
#include "usb/classdriver/mouse.hpp"
#include "layer.hpp"

namespace {
  // Mouse shape
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };
}

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {255, 255, 255});
      } else {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, kMouseTransparentColor);
      }
    }
  }
}

Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {
}

void Mouse::SetPosition(Vector2D<int> position) {
  position_ = position;
  layer_manager->Move(layer_id_, position_);
}

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  const auto oldpos = position_;
  // move the mouse cursor window
  auto newpos = position_ + Vector2D<int>{displacement_x, displacement_y};
  // limit mouse cursor move inside the screen area
  position_ = ElementMax(ElementMin(newpos, ScreenSize() + Vector2D<int>{-1, -1}), {0, 0});

  const auto posdiff = position_ - oldpos;

  layer_manager->Move(layer_id_, position_);

  const bool previous_left_pressed = (previous_buttons_ & 0x01);
  const bool left_pressed = (buttons & 0x01);
  if (!previous_left_pressed && left_pressed) {
    auto layer = layer_manager->FindLayerByPosition(position_, layer_id_);
    if (layer && layer->IsDraggable()) {
      drag_layer_id_ = layer->ID();
      active_layer->Activate(layer->ID());
    } else {
      active_layer->Activate(0);
    }
  } else if (previous_left_pressed && left_pressed) {
    if (drag_layer_id_ > 0) {
      layer_manager->MoveRelative(drag_layer_id_, posdiff);
    }
  } else if (previous_left_pressed && !left_pressed) {
    drag_layer_id_ = 0;
  }

  previous_buttons_ = buttons;
}

void InitializeMouse() {
  // create new window for mouse cursor
  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  // save mouse cursor desgin data to mouse_window
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .ID();

  auto mouse = std::make_shared<Mouse>(mouse_layer_id);
  mouse->SetPosition({10, 20});
  layer_manager->UpDown(mouse_layer_id, std::numeric_limits<int>::max());

  // set mouse callback method
  usb::HIDMouseDriver::default_observer =
    [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
      mouse->OnInterrupt(buttons, displacement_x, displacement_y);
    };

  active_layer->SetMouseLayer(mouse_layer_id);
}