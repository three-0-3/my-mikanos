#pragma once

#include <optional>
#include <vector>

#include "graphics.hpp"
#include "frame_buffer.hpp"

class Window {
	public:
		class WindowWriter : public PixelWriter {
			public:
				WindowWriter(Window& window) : window_{window} {}
				// update the pixel data at (x, y) with color c
				virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
					window_.Write(pos, c);
				}
				virtual int Width() const override { return window_.Width(); }
				virtual int Height() const override { return window_.Height(); }

			private:
				Window& window_;
		};

		// constructor
		Window(int width, int height, PixelFormat shadow_format);
		// deconstructor
		~Window() = default;
		// omit copy constructor
		Window(const Window& rhs) = delete;
		// omit copy assignment operator
		Window& operator=(const Window& rhs) = delete;

		// write the data saved in window with position offset
		void DrawTo(FrameBuffer& dst, Vector2D<int> pos);
		// set the transparent color
		void SetTransparentColor(std::optional<PixelColor> c);
		// window writer associated with this window
		WindowWriter* Writer();

		// get the pixel data at the specified position
		const PixelColor& At(Vector2D<int> pos) const;
		// write to the pixel data at the specified position and update shadow buffer
		void Write(Vector2D<int> pos, PixelColor c);


	  // get width of the drawing area by pixel
		int Width() const;
		// get height of the drawing area by pixel
		int Height() const;

		// Move rectangle in Window area
		void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

	private:
	  // width/height of the drawing area of this window by pixel
		int width_, height_;
		// pixel data saved in this window 
		std::vector<std::vector<PixelColor>> data_{};
		// window writer associated with this window
		WindowWriter writer_{*this};
		// if the pixel color equals to this color, the pixel does not be drawn
		std::optional<PixelColor> transparent_color_{std::nullopt};

		FrameBuffer shadow_buffer_{};
};

void DrawWindow(PixelWriter& writer, const char* title);