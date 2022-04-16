#include "frame_buffer.hpp"

#include "graphics.hpp"
#include "error.hpp"

Error FrameBuffer::Initialize(const FrameBufferConfig& config) {
	config_ = config;

	const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
	// minus value means there is no match for the given pixel format
	if (bits_per_pixel <= 0) {
		return MAKE_ERROR(Error::kUnknownPixelFormat);
	}

	// true when this FrameBuffer object is for "real" frame buffer (i.e. VRAM)
	if (config_.frame_buffer) {
		// FrameBuffer object for VRAM does not need buffer_ space
		buffer_.resize(0);
	// false when this FrameBuffer object is shadow buffer of Window class
	} else {
		buffer_.resize(
			((bits_per_pixel + 7) / 8)
			* config_.horizontal_resolution * config_.vertical_resolution);
		// save the top address of buffer_ to config_
		config_.frame_buffer = buffer_.data();
		// pixels_per_scan_line is set to width of Window
		config_.pixels_per_scan_line = config_.horizontal_resolution;
	}

	switch (config_.pixel_format)	{
		case kPixelRGBResv8BitPerColor:
			writer_ = std::make_unique<RGBResv8BitPerColorPixelWriter>(config_);
			break;
		case kPixelBGRResv8BitPerColor:
			writer_ = std::make_unique<BGRResv8BitPerColorPixelWriter>(config_);
			break;
	default:
			return MAKE_ERROR(Error::kUnknownPixelFormat);
	}	

	return MAKE_ERROR(Error::kSuccess);
}

Error FrameBuffer::Copy(Vector2D<int> pos, const FrameBuffer& src) {
	// if the file formats of src and dst are different, copy is not allowed to prevent color misconversion
	if (config_.pixel_format != src.config_.pixel_format) {
		return MAKE_ERROR(Error::kUnknownPixelFormat);
	}

	const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
	if (bits_per_pixel <= 0) {
		return MAKE_ERROR(Error::kUnknownPixelFormat);
	}
	// rename variables to shorthands
	const auto dst_width = config_.horizontal_resolution;
	const auto dst_height = config_.vertical_resolution;
	const auto src_width = src.config_.horizontal_resolution;
	const auto src_height = src.config_.vertical_resolution;

	const int copy_start_dst_x = std::max(pos.x, 0);
	const int copy_start_dst_y = std::max(pos.y, 0);
	const int copy_end_dst_x = std::min(pos.x + src_width, dst_width);
	const int copy_end_dst_y = std::min(pos.y + src_height, dst_height);

	const auto bytes_per_pixel = (bits_per_pixel + 7)/8;
	const auto bytes_per_copy_line = bytes_per_pixel * (copy_end_dst_x - copy_start_dst_x);
	
	uint8_t* dst_buf = config_.frame_buffer + bytes_per_pixel * (config_.pixels_per_scan_line * copy_start_dst_y + copy_end_dst_x);
	const uint8_t* src_buf = src.config_.frame_buffer;

	for (int dy = 0; dy < copy_end_dst_y - copy_start_dst_y; ++dy) {
		memcpy(dst_buf, src_buf, bytes_per_copy_line);
		dst_buf += bytes_per_pixel * config_.pixels_per_scan_line;
		src_buf += bytes_per_pixel * src.config_.pixels_per_scan_line;
	}

	return MAKE_ERROR(Error::kSuccess);
}

int BitsPerPixel(PixelFormat format) {
	switch (format) {
		case kPixelRGBResv8BitPerColor: return 32;
		case kPixelBGRResv8BitPerColor: return 32;
	}
	return -1;
}