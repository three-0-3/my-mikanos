#include "graphics.hpp"

// the variables declared in the object file (hankaku.o) created by objcopy
extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_size;

// get address of the requested characters in the font binary file
const uint8_t* GetFont(char c) {
  // convert char to int and multiply by 16 (16 bytes per 1 char)
  auto index = 16 * static_cast<unsigned int>(c);

  // if requested char is out of the font file scope, return null pointer
  // to take the addresss and cast to int since the size is stored as the pseudo-address in elf file
  if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {
    return nullptr;
  }

  // add the offset of font file start address and return address of the requeted char
  return &_binary_hankaku_bin_start + index;
}

void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color) {
  // get the font data address of the char to write
  const uint8_t* font = GetFont(c);
  if (font == nullptr) {
    return;
  }
  // according to the font data, write the pixels one by one
  for (int dy = 0; dy < 16; ++dy) { // 1 char consist of 16 lines
    for (int dx = 0; dx < 8; ++dx) { // 1 line consist of 8 pixels
      // fill the pixel only when the focused bit equals to 1
      if ((font[dy] << dx) & 0x80u) { 
        writer.Write(x + dx, y + dy, color);
      }
    }
  }
}