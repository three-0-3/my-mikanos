#include "layer.hpp"

#include "console.hpp"
#include "logger.hpp"

Layer::Layer(unsigned int id) : id_{id} {}

unsigned int Layer::ID() const {
	return id_;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
	window_ = window;
	return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const {
	return window_;
}

Vector2D<int> Layer::GetPosition() const {
	return pos_;
}

Layer& Layer::SetDraggable(bool draggable) {
	draggable_ = draggable;
	return *this;
}

bool Layer::IsDraggable() const {
	return draggable_;
}

Layer& Layer::Move(Vector2D<int> pos) {
	pos_ = pos;
	return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
	pos_ += pos_diff;
	return *this;
}

void Layer::DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const {
	if (window_) {
		window_->DrawTo(screen, pos_, area);
	}
}

void LayerManager::SetWriter(FrameBuffer* screen) {
	screen_ = screen;

	// Initialize back buffer
	FrameBufferConfig back_config = screen->Config();
	back_config.frame_buffer = nullptr;
	back_buffer_.Initialize(back_config);
}

Layer& LayerManager::NewLayer() {
	++latest_id_;
	return *layers_.emplace_back(new Layer{latest_id_});
}

void LayerManager::Draw(const Rectangle<int>& area) const {
  // draw each layer to the back buffer first then copy to frame buffer to prevent flickering
	for (auto layer : layer_stack_) {
		layer->DrawTo(back_buffer_, area);
	}
	screen_->Copy(area.pos, back_buffer_, area);
}

void LayerManager::Draw(unsigned int id) const {
	bool draw = false;
	Rectangle<int> window_area;
	for (auto layer : layer_stack_) {
		if (layer->ID() == id) {
			window_area.size = layer->GetWindow()->Size();
			window_area.pos = layer->GetPosition();
			draw = true;
		}
		// draw the layer of specified id and above
		if (draw) {
			// draw each layer to the back buffer first then copy to frame buffer to prevent flickering
			layer->DrawTo(back_buffer_, window_area);
		}
	}
	screen_->Copy(window_area.pos, back_buffer_, window_area);
}

void LayerManager::Move(unsigned int id, Vector2D<int> new_pos) {
	auto layer = FindLayer(id);
	const auto window_size = layer->GetWindow()->Size();
	const auto old_pos = layer->GetPosition();
	layer->Move(new_pos);
	// Re-draw the source area
	Draw({old_pos, window_size});
	// Re-draw the destination area
	Draw(id);
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
	auto layer = FindLayer(id);
	const auto window_size = layer->GetWindow()->Size();
	const auto old_pos = layer->GetPosition();
	layer->MoveRelative(pos_diff);
	// Re-draw the source area
	Draw({old_pos, window_size});
	// Re-draw the destination area
	Draw(id);
}

void LayerManager::UpDown(unsigned int id, int new_height) {
	// if the new height is minus, the layer is hidden
	if (new_height < 0) {
		Hide(id);
		return;
	}
	// if the new height is bigger than the current layer, the layer is placed on the top
	if (new_height > layer_stack_.size()) {
		new_height = layer_stack_.size();
	}

	auto layer = FindLayer(id);
	auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
	auto new_pos = layer_stack_.begin() + new_height;

	// if the layer is not in the visible layer list, just insert and finish
	if (old_pos == layer_stack_.end()) {
		layer_stack_.insert(new_pos, layer);
		return;
	}

	// if the layer is in the visible layer list already and the new pos is at the top,
	// decrement new pos
	if (new_pos == layer_stack_.end()) {
		--new_pos;
	}
	// if the layer is in the visible layer list already and the new pos is not at the top,
	// do nothing

	layer_stack_.erase(old_pos);
	layer_stack_.insert(new_pos, layer);
}

void LayerManager::Hide(unsigned int id) {
	auto layer = FindLayer(id);
	auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
	// if layer equals to layer_stack_.end(), the layer was not found
	if (pos != layer_stack_.end()) {
		layer_stack_.erase(pos);
	}
}

Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) {
	auto pred = [pos, exclude_id](Layer* layer) {
		// if current layer id is excluded, go to next layer
		if (layer->ID() == exclude_id) {
			return false;
		}
		// if current layer does not have window, go to next layer
		const auto win = layer->GetWindow();
		if (!win) {
			return false;
		}
		// if window on the current layer includes the specified pos, this layer is what we are looking for
		const auto win_pos = layer->GetPosition();
		const auto win_end_pos = win_pos + win->Size();
		return win_pos.x <= pos.x && pos.x < win_end_pos.x &&
		       win_pos.y <= pos.y && pos.y < win_end_pos.y;
	};
	auto it = std::find_if(layer_stack_.rbegin(), layer_stack_.rend(), pred);
	if (it == layer_stack_.rend()) {
		return nullptr;
	}
	return *it;
}

Layer* LayerManager::FindLayer(unsigned int id) {
	// define the lambda function to check the element's id equlas to target id
	auto pred = [id](const std::unique_ptr<Layer>& elem) {
		return elem->ID() == id;
	};
	auto it = std::find_if(layers_.begin(), layers_.end(), pred);
	if (it == layers_.end()) {
		return nullptr;
	}
	// get() because find_if returns iterator
	return it->get();
}

LayerManager* layer_manager;

void InitializeLayer() {
	const auto screen_size = ScreenSize();

	auto bgwindow = std::make_shared<Window>(screen_size.x, screen_size.y, screen_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  // save background design data to bgwindow
  DrawDesktop(*bgwriter);

  auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
  console->SetWindow(console_window);

  FrameBuffer* screen = new FrameBuffer;
  if (auto err = screen->Initialize(screen_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
  }

  // create layer manager to control all the layers/windows
  layer_manager = new LayerManager;
  // set pixel writer (not window writer) to layer manager
  layer_manager->SetWriter(screen);


  // add new layer for background window
  auto bglayer_id = layer_manager->NewLayer()
    .SetWindow(bgwindow)
    .Move({0, 0})
    .ID();
  console->SetLayerID(layer_manager->NewLayer()
    .SetWindow(console_window)
    .Move({0,0})
    .ID());

  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(console->LayerID(), 1);
}