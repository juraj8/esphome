#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace nt7538 {

class NT7538 : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_200KHZ> {
 public:
  void set_height(uint16_t height) { this->height_ = height; }
  void set_width(uint16_t width) { this->width_ = width; }

  void set_dc_pin(GPIOPin *value) { this->dc_pin_ = value; }
  void set_reset_pin(GPIOPin *value) { this->reset_pin_ = value; }
  void set_backlight_pin(GPIOPin *backlight_pin) { this->backlight_pin_ = backlight_pin; }

  void display_chess(uint8_t value);
  void display_picture(uint8_t pic[]);

  void backlight(bool onoff);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  // void loop() override;

  void write_display_data();

 protected:
  int16_t width_ = 128, height_ = 64;
  uint8_t contrast_level_ = 0x22;

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};

  void init_reset_();
  void display_init_();

  void set_page_address(uint8_t add);
  void set_column_address(uint8_t add);

  void power_control(uint8_t vol);
  void set_reg_resistor(uint8_t r);
  void set_contrast_control_register(uint8_t mod);
  void set_start_line(uint8_t line);
  void set_static_indicator(bool enabled, uint8_t mod);

  void command_(uint8_t value);
  void data_(uint8_t value);

  int get_width_internal() override;
  int get_height_internal() override;
  size_t get_buffer_length_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
};

}  // namespace nt7538
}  // namespace esphome
