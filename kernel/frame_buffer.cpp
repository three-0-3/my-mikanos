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

int BitsPerPixel(PixelFormat format) {
	switch (format) {
		case kPixelRGBResv8BitPerColor: return 32;
		case kPixelBGRResv8BitPerColor: return 32;
	}
	return -1;
}