#include "console.hpp"

#include <cstring>
#include "font.hpp"
#include "layer.hpp"

// Initialize class variables with arguments
// cursor initial position is (0, 0)
Console::Console(PixelWriter* writer, const PixelColor& fg_color, const PixelColor& bg_color)
	: writer_{writer}, fg_color_{fg_color}, bg_color_{bg_color},
	  buffer_{}, cursor_row_{0}, cursor_column_{0} {}

void Console::PutString(const char* s) {
	while (*s) {
		if (*s == '\n') {
		// if it is the new line character, process it
			Newline();
		} else if (cursor_column_ < kColumns - 1) {
		// if the cursor is not in the last column
		  // write the character
			WriteAscii(*writer_, Vector2D<int>{8 * cursor_column_, 16 * cursor_row_}, *s, fg_color_);
			// save it to the buffer
			buffer_[cursor_row_][cursor_column_] = *s;
			// move the cursor to right
			++cursor_column_;
		}
		++s;
		// if the cursor is in the last column,
		// the input character is not shown and ignored until the new line is input
	}
	if (layer_manager) {
		layer_manager->Draw();
	}
}

void Console::SetWriter(PixelWriter* writer) {
	writer_ = writer;
	Refresh();
}

void Console::Newline() {
	cursor_column_ = 0;
	if (cursor_row_ < kRows - 1) {
	// if the cursor is not in the last row, just move cursor to the next line
		++cursor_row_;
	} else {
	// if the cursor is in the last row, scroll one row
		// refresh the whole screen
		for (int y = 0; y < 16 * kRows; ++y) {
			for (int x = 0; x < 8 * kColumns; ++x) {
				writer_->Write(Vector2D<int>{x, y}, bg_color_);
			}
		}
		for (int row = 0; row < kRows - 1; ++row) {
			// move the buffer to the previous row
			memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
			// write again from buffer
			WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
		}
		// delete the buffer of the last row
		memset(buffer_[kRows - 1], 0, kColumns + 1);
	}
}

void Console::Refresh() {
	for (int row = 0; row < kRows; ++row) {
		WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
	}
}