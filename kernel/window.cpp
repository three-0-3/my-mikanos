#include "window.hpp"

#include "logger.hpp"
#include "font.hpp"

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
	// set the size of the vector class
	data_.resize(height);
	for (int y = 0; y < height; ++y) {
		data_[y].resize(width);
	}

	FrameBufferConfig config{};
	config.frame_buffer = nullptr; // to be set at FrameBuffer's initializer
	config.horizontal_resolution = width;
	config.vertical_resolution = height;
	config.pixel_format = shadow_format;

	if (auto err = shadow_buffer_.Initialize(config)) {
		Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
				err.Name(), err.File(), err.Line());
	}
}

void Window::DrawTo(FrameBuffer& dst, Vector2D<int> pos) {
	// if transparent color is not set, just draw all the pixel
	if (!transparent_color_) {
		dst.Copy(pos, shadow_buffer_);
		return;
	}

	// if transparent color is set in this window, not draw the pixel if the color equals that color
	const auto tc = transparent_color_.value();
	auto& writer = dst.Writer();
	// for (int y = 0; y < Height(); ++y) {
	// 	for (int x = 0; x < Width(); ++x) {
	for (int y = std::max(0, -pos.y);
			 y < std::min(Height(), writer.Height() - pos.y);
			 ++y) {
		for (int x = std::max(0, -pos.x);
		    x < std::min(Width(), writer.Width() - pos.x);
				++x) {
			const auto c = At(Vector2D<int>{x, y});
			if (c != tc) {
				writer.Write(pos + Vector2D<int>{x, y}, c);
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

const PixelColor& Window::At(Vector2D<int> pos) const{
	return data_[pos.y][pos.x];
}

void Window::Write(Vector2D<int> pos, PixelColor c) {
	data_[pos.y][pos.x] = c;
	shadow_buffer_.Writer().Write(pos, c);
}

int Window::Width() const {
	return width_;
}

int Window::Height() const {
	return height_;
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
	shadow_buffer_.Move(dst_pos, src);
}

namespace {
	const int kCloseButtonWidth = 16;
	const int kCloseButtonHeight = 14;
	const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
    "...............@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".:::@@::::@@::$@",
    ".::::@@::@@:::$@",
    ".:::::@@@@::::$@",
    ".::::::@@:::::$@",
    ".:::::@@@@::::$@",
    ".::::@@::@@:::$@",
    ".:::@@::::@@::$@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".$$$$$$$$$$$$$$@",
    "@@@@@@@@@@@@@@@@",
	};

	constexpr PixelColor ToColor(uint32_t c) {
		return {
			static_cast<uint8_t>(c >> 16),
			static_cast<uint8_t>(c >> 8),
			static_cast<uint8_t>(c)
		};
	}
}

void DrawWindow(PixelWriter& writer, const char* title) {
	auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
		FillRectangle(writer, pos, size, ToColor(c));
	};
	const auto win_w = writer.Width();
	const auto win_h = writer.Height();

	// horizontal lines on the top
  fill_rect({0, 0},         {win_w, 1},             0xc6c6c6);
  fill_rect({1, 1},         {win_w - 2, 1},         0xffffff);
	// vertical lines on the left
  fill_rect({0, 0},         {1, win_h},             0xc6c6c6);
  fill_rect({1, 1},         {1, win_h - 2},         0xffffff);
	// vertical lines on the right
  fill_rect({win_w - 2, 1}, {1, win_h - 2},         0x848484);
  fill_rect({win_w - 1, 0}, {1, win_h},             0x000000);
	// fill background of the window
  fill_rect({2, 2},         {win_w - 4, win_h - 4}, 0xc6c6c6);
	// fill title bar
  fill_rect({3, 3},         {win_w - 6, 18},        0x000084);
	// horizontal lines on the bottom
  fill_rect({1, win_h - 2}, {win_w - 2, 1},         0x848484);
  fill_rect({0, win_h - 1}, {win_w, 1},             0x000000);

	// write the title on the top
	WriteString(writer, {24, 4}, title, ToColor(0xffffff));

	// draw the close button on the right top
	for (int y = 0; y < kCloseButtonHeight; ++y) {
		for (int x = 0; x < kCloseButtonWidth; ++x) {
			PixelColor c = ToColor(0xffffff);
			if (close_button[y][x] == '@') {
				c = ToColor(0x000000);
			} else if (close_button[y][x] == '$') {
				c = ToColor(0x848484);
			} else if (close_button[y][x] == ':') {
				c = ToColor(0xc6c6c6);
			}
			writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, c);
		}
	}
}