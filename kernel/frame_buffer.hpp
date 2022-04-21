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
		Error Copy(Vector2D<int> dst_pos, const FrameBuffer& src, const Rectangle<int>& src_area);
		void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

	  FrameBufferWriter& Writer() { return *writer_; }

	private:
		FrameBufferConfig config_{};
		std::vector<uint8_t> buffer_{};
		// unique_ptr as this FrameBuffer owns FrameBufferWriter
		std::unique_ptr<FrameBufferWriter> writer_{};
};