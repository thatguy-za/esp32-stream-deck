#include "stream_deck_canvas.h"

namespace esphome {
namespace stream_deck {

namespace {
void put_u16_le(uint8_t *out, uint16_t v) {
  out[0] = v & 0xFF;
  out[1] = (v >> 8) & 0xFF;
}
void put_u32_le(uint8_t *out, uint32_t v) {
  out[0] = v & 0xFF;
  out[1] = (v >> 8) & 0xFF;
  out[2] = (v >> 16) & 0xFF;
  out[3] = (v >> 24) & 0xFF;
}
}  // namespace

void StreamDeckCanvas::init(int width, int height) {
  this->width_ = width;
  this->height_ = height;
  this->init_internal_(static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 3);
}

void StreamDeckCanvas::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_ || this->buffer_ == nullptr) {
    return;
  }
  size_t idx = (static_cast<size_t>(y) * this->width_ + x) * 3;
  this->buffer_[idx + 0] = color.r;
  this->buffer_[idx + 1] = color.g;
  this->buffer_[idx + 2] = color.b;
}

Color StreamDeckCanvas::get_pixel_(int x, int y) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_ || this->buffer_ == nullptr) {
    return Color(0, 0, 0);
  }
  size_t idx = (static_cast<size_t>(y) * this->width_ + x) * 3;
  return Color(this->buffer_[idx + 0], this->buffer_[idx + 1], this->buffer_[idx + 2]);
}

std::vector<uint8_t> StreamDeckCanvas::encode_bmp() const {
  const int w = this->width_;
  const int h = this->height_;
  const uint32_t row_size = ((static_cast<uint32_t>(w) * 3 + 3) / 4) * 4;  // rows padded to 4 bytes
  const uint32_t pixel_data_size = row_size * static_cast<uint32_t>(h);
  const uint32_t file_size = 54 + pixel_data_size;

  std::vector<uint8_t> out(file_size, 0);

  // File header (14 bytes)
  out[0] = 'B';
  out[1] = 'M';
  put_u32_le(&out[2], file_size);
  put_u32_le(&out[10], 54);  // pixel data offset

  // DIB header (BITMAPINFOHEADER, 40 bytes). Height is negative so rows are
  // stored top-down in the file - we apply the Mini's documented transform
  // ourselves below instead of relying on BMP's default bottom-up order,
  // to keep exactly one, unambiguous flip/rotate step.
  put_u32_le(&out[14], 40);
  put_u32_le(&out[18], static_cast<uint32_t>(w));
  put_u32_le(&out[22], static_cast<uint32_t>(-h));
  put_u16_le(&out[26], 1);   // planes
  put_u16_le(&out[28], 24);  // bits per pixel
  put_u32_le(&out[30], 0);   // BI_RGB, uncompressed
  put_u32_le(&out[34], pixel_data_size);

  uint8_t *pixels = &out[54];
  for (int row = 0; row < h; row++) {
    uint8_t *row_ptr = pixels + static_cast<size_t>(row) * row_size;
    for (int col = 0; col < w; col++) {
      // Flip vertical + rotate 90 (see this function's doc comment on the
      // uncertainty here) - implemented as a transpose, which combines
      // both for a square canvas.
      Color c = this->get_pixel_(row, col);
      // BMP stores BGR, not RGB.
      row_ptr[col * 3 + 0] = c.b;
      row_ptr[col * 3 + 1] = c.g;
      row_ptr[col * 3 + 2] = c.r;
    }
  }

  return out;
}

}  // namespace stream_deck
}  // namespace esphome
