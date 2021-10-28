#pragma once

#include "graphics.hpp"

class Console {
	public:
	  // console size (number of characters shown at at time)
		static const int kRows = 25, kColumns = 80;
	
		Console(PixelWriter* writer, const PixelColor& fg_color, const PixelColor& bg_color);
		void PutString(const char* s);
		void SetWriter(PixelWriter* writer);

	private:
		void Newline();
		void Refresh();

		PixelWriter* writer_;
		// fg color for character and bg color for background
		const PixelColor fg_color_, bg_color_;
		// buffer column size is 1 byte bigger than kColumn for null char
		char buffer_[kRows][kColumns + 1];
		// cursor position
		int cursor_row_, cursor_column_;
};