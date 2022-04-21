#pragma once

#include "graphics.hpp"
#include "window.hpp"

class Console {
	public:
	  // console size (number of characters shown at at time)
		static const int kRows = 25, kColumns = 80;
	
		Console(const PixelColor& fg_color, const PixelColor& bg_color);
		void PutString(const char* s);
		void SetWriter(PixelWriter* writer);
		void SetWindow(const std::shared_ptr<Window>& window);
		void SetLayerID(unsigned int layer_id_);
		unsigned int LayerID() const;

	private:
		void Newline();
		void Refresh();

		PixelWriter* writer_;
		std::shared_ptr<Window> window_;
		// fg color for character and bg color for background
		const PixelColor fg_color_, bg_color_;
		// buffer column size is 1 byte bigger than kColumn for null char
		char buffer_[kRows][kColumns + 1];
		// cursor position
		int cursor_row_, cursor_column_;
		unsigned int layer_id_;
};