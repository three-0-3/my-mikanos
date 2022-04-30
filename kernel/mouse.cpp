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

// mouse layer id defined here to be used in MouseObserver
unsigned int mouse_layer_id;
Vector2D<int> mouse_position;

void MouseObserver(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  static unsigned int mouse_drag_layer_id = 0;
  static uint8_t previous_buttons = 0;

  const auto oldpos = mouse_position;
  // move the mouse cursor window
  auto newpos = mouse_position + Vector2D<int>{displacement_x, displacement_y};
  // limit mouse cursor move inside the screen area
  mouse_position = ElementMax(ElementMin(newpos, ScreenSize() + Vector2D<int>{-1, -1}), {0, 0});

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

void InitializeMouse() {
  // create new window for mouse cursor
  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  // save mouse cursor desgin data to mouse_window
  DrawMouseCursor(mouse_window->Writer(), {0, 0});
  mouse_position = {200, 200};

  mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .Move(mouse_position)
    .ID();
  layer_manager->UpDown(mouse_layer_id, std::numeric_limits<int>::max());

	// set mouse callback method
  usb::HIDMouseDriver::default_observer = MouseObserver;
}