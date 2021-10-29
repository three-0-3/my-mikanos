#include "window.hpp"

Window::Window(int width, int height) : width_{width}, height_{height} {
	// set the size of the vector class
	data_.resize(height);
	for (int y = 0; y < height; ++y) {
		data_[y].resize(width);
	}
}

void Window::DrawTo(PixelWriter& writer, Vector2D<int> position) {
	// if transparent color is not set, just draw all the pixel
	if (!transparent_color_) {
		for (int y = 0; y < Height(); ++y) {
			for (int x = 0; x < Width(); ++x) {
				writer.Write(position + Vector2D<int>{x, y}, At(Vector2D<int>{x, y}));
			}
		}
		return;
	}

	// if transparent color is set in this window, not draw the pixel if the color equals that color
	const auto tc = transparent_color_.value();
	for (int y = 0; y < Height(); ++y) {
		for (int x = 0; x < Width(); ++x) {
			const auto c = At(Vector2D<int>{x, y});
			if (c != tc) {
				writer.Write(position + Vector2D<int>{x, y}, c);
			}
		} 
	}
}

void Window::SetTransparentColor(std::optional<PixelColor> c) {
	transparent_color_ = c;
}

Window::WindowWriter* Window::Writer() {
	return &writer_;
}

PixelColor& Window::At(Vector2D<int> pos) {
	return data_[pos.y][pos.x];
}

int Window::Width() const {
	return width_;
}

int Window::Height() const {
	return height_;
}