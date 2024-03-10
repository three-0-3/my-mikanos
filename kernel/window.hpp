#pragma once

#include <optional>
#include <vector>
#include <string>

#include "graphics.hpp"
#include "frame_buffer.hpp"

enum class WindowRegion {
  kTitleBar,
  kCloseButton,
  kBorder,
  kOther,
};

class Window {
  public:
    class WindowWriter : public PixelWriter {
      public:
        WindowWriter(Window& window) : window_{window} {}
        // update the pixel data at (x, y) with color c
        virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
          window_.Write(pos, c);
        }
        virtual int Width() const override { return window_.Width(); }
        virtual int Height() const override { return window_.Height(); }

      private:
        Window& window_;
    };

    // constructor
    Window(int width, int height, PixelFormat shadow_format);
    // deconstructor
    virtual ~Window() = default;
    // omit copy constructor
    Window(const Window& rhs) = delete;
    // omit copy assignment operator
    Window& operator=(const Window& rhs) = delete;

    // write the data saved in window with position offset
    void DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area);
    // set the transparent color
    void SetTransparentColor(std::optional<PixelColor> c);
    // window writer associated with this window
    WindowWriter* Writer();

    // get the pixel data at the specified position
    const PixelColor& At(Vector2D<int> pos) const;
    // write to the pixel data at the specified position and update shadow buffer
    void Write(Vector2D<int> pos, PixelColor c);


    // get width of the drawing area by pixel
    int Width() const;
    // get height of the drawing area by pixel
    int Height() const;
    // get size of the drawing area by pixel
    Vector2D<int> Size() const;

    // Move rectangle in Window area
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

    virtual void Activate() {}
    virtual void Deactivate() {}

    virtual WindowRegion GetWindowRegion(Vector2D<int> pos);

  private:
    // width/height of the drawing area of this window by pixel
    int width_, height_;
    // pixel data saved in this window 
    std::vector<std::vector<PixelColor>> data_{};
    // window writer associated with this window
    WindowWriter writer_{*this};
    // if the pixel color equals to this color, the pixel does not be drawn
    std::optional<PixelColor> transparent_color_{std::nullopt};

    FrameBuffer shadow_buffer_{};
};

class ToplevelWindow : public Window {
  public:
    static constexpr Vector2D<int> kTopLeftMargin{4, 24};
    static constexpr Vector2D<int> kBottomRightMargin{4,4};
    static constexpr int kMarginX = kTopLeftMargin.x + kBottomRightMargin.x;
    static constexpr int kMarginY = kTopLeftMargin.y + kBottomRightMargin.y;

    class InnerAreaWriter : public PixelWriter {
      public:
        InnerAreaWriter(ToplevelWindow& window) : window_{window} {}
        virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
          window_.Write(pos + kTopLeftMargin, c);
        }
        virtual int Width() const override {
          return window_.Width() - kTopLeftMargin.x - kBottomRightMargin.x; }
        virtual int Height() const override {
          return window_.Height() - kTopLeftMargin.y - kBottomRightMargin.y; }
        
      private:
        ToplevelWindow& window_;
    };

    ToplevelWindow(int width, int height, PixelFormat shadow_format,
                   const std::string& title);

    virtual void Activate() override;
    virtual void Deactivate() override;
    virtual WindowRegion GetWindowRegion(Vector2D<int> pos) override;

    InnerAreaWriter* InnerWriter() { return &inner_writer_; }
    Vector2D<int> InnerSize() const;

  private:
    std::string title_;
    InnerAreaWriter inner_writer_{*this};
};

void DrawWindow(PixelWriter& writer, const char* title);
void DrawTextbox(PixelWriter& writer, Vector2D<int>pos, Vector2D<int> size);
void DrawTerminal(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size);
void DrawWindowTitle(PixelWriter& writer, const char* title, bool active);