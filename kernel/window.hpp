#pragma once

#include <optional>
#include <vector>

#include "graphics.hpp"

class Window {
	public:
		class WindowWriter : public PixelWriter {
			public:
				WindowWriter(Window& window) : window_{window} {}
				// update the pixel data at (x, y) with color c
				virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
					window_.At(pos) = c;
				}
				virtual int Width() const override { return window_.Width(); }
				virtual int Height() const override { return window_.Height(); }

			private:
				Window& window_;
		};

		// constructor
		Window(int width, int height);
		// deconstructor
		~Window() = default;
		// omit copy constructor
		Window(const Window& rhs) = delete;
		// omit copy assignment operator
		Window& operator=(const Window& rhs) = delete;

		// write the data saved in window with position offset
		void DrawTo(PixelWriter& writer, Vector2D<int> position);
		// set the transparent color
		void SetTransparentColor(std::optional<PixelColor> c);
		// window writer associated with this window
		WindowWriter* Writer();

		// get the pixel data at the specified position
		PixelColor& At(Vector2D<int> pos);
		// get the pixel data at the specified position
		const PixelColor& At(Vector2D<int> pos) const;

	  // get width of the drawing area by pixel
		int Width() const;
		// get height of the drawing area by pixel
		int Height() const;

	private:
	  // width/height of the drawing area of this window by pixel
		int width_, height_;
		// pixel data saved in this window 
		std::vector<std::vector<PixelColor>> data_{};
		// window writer associated with this window
		WindowWriter writer_{*this};
		// if the pixel color equals to this color, the pixel does not be drawn
		std::optional<PixelColor> transparent_color_{std::nullopt};
};