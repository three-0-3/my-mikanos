#pragma once

#include <memory>
#include <vector>

#include "error.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"

class FrameBuffer {
	public:
		// Initialize frame buffer by checking config
		Error Initialize(const FrameBufferConfig& config);
		Error Copy(Vector2D<int> pos, const FrameBuffer& src);

	  FrameBufferWriter& Writer() { return *writer_; }

	private:
		FrameBufferConfig config_{};
		std::vector<uint8_t> buffer_{};
		// unique_ptr as this FrameBuffer owns FrameBufferWriter
		std::unique_ptr<FrameBufferWriter> writer_{};
};