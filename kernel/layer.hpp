#pragma once

#include "graphics.hpp"
#include "window.hpp"
#include "frame_buffer.hpp"

class Layer {
	public:
		// constructor
		Layer(unsigned int id = 0);
		// get id of this instance
		unsigned int ID() const;

		// set the window to this layer (if existing, old is gone)
		Layer& SetWindow(const std::shared_ptr<Window>& window);
		// get the window
		std::shared_ptr<Window> GetWindow() const;
		// get the layer's origin position
		Vector2D<int> GetPosition() const;

		// update the layer's position to specified absolute value (no draw again)
		Layer& Move(Vector2D<int> pos);
		// update the layer's position to specified relative value (no draw again)
		Layer& MoveRelative(Vector2D<int> pos_diff);

		// write the pixel in window data by writer
		void DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const;

	private:
		unsigned int id_;
		Vector2D<int> pos_;
		std::shared_ptr<Window> window_;
};

class LayerManager {
	public:
		// set the writer for DrawTo method
		void SetWriter(FrameBuffer* screen);
		// create new layer and save in layer manager
		Layer& NewLayer();

		// draw the layers (only specified area)
		void Draw(const Rectangle<int>& area) const;
		// draw the layers (only the layers specified and above)
		void Draw(unsigned int id) const;

		// update the layer's position to specified absolute value (re-draw included)
		void Move(unsigned int id, Vector2D<int> new_pos);
		// update the layer's position to specified relative value (re-draw included)
		void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

		// move the layer's drawing order to the specified new height
		// if the new height is minus, the layer is hidden
		// if the new height is bigger than the current layer, the layer is placed on the top
		void UpDown(unsigned int id, int new_height);
		// hide the layer
		void Hide(unsigned int id);

	private:
		FrameBuffer* screen_{nullptr};
		// buffer of the screen size to prevent flickering
		mutable FrameBuffer back_buffer_{};
		// container to keep all the layers (including hidden layers)
		std::vector<std::unique_ptr<Layer>> layers_{};
		// container to keep the visible layers only
		std::vector<Layer*> layer_stack_{};
		// most-recent added layer id
		unsigned int latest_id_{0};

		// find the layer with specified id
		Layer* FindLayer(unsigned int id);
};

extern LayerManager* layer_manager;