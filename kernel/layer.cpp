#include "layer.hpp"

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
}

Layer& LayerManager::NewLayer() {
	++latest_id_;
	return *layers_.emplace_back(new Layer{latest_id_});
}

void LayerManager::Draw(const Rectangle<int>& area) const {
	for (auto layer : layer_stack_) {
		layer->DrawTo(*screen_, area);
	}
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
		if (draw) {
			layer->DrawTo(*screen_, window_area);
		}
	}
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