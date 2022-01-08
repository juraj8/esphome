#include "nt7538.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace nt7538 {

static const char *const TAG = "nt7538";

#define DISPLAY_ON() command_(NT7538_COMMAND_DISPLAY_ON)        //  Display on
#define DISPLAY_OFF() command_(NT7538_COMMAND_DISPLAY_OFF)      //  Display off
#define SET_ADC_REVERSE() command_(NT7538_COMMAND_ADC_REVERSE)  //  Reverse disrect (SEG131-SEG0)
#define SET_ADC_NORMAL() command_(NT7538_COMMAND_ADC_NORMAL)    //  Normal disrect (SEG0-SEG131)

#define REVERSE_DISPLAY_OFF() command_(NT7538_COMMAND_DISP_NORMAL)  //  Normal display : 1 illuminated
#define REVERSE_DISPLAY_ON() command_(NT7538_COMMAND_DISP_REVERSE)  //  Reverse display : 0 illuminated

#define ENTIRE_DISPLAY_OFF() command_(NT7538_COMMAND_ENTIRE_DISP_OFF)  //  Normal display
#define ENTIRE_DISPLAY_ON() command_(NT7538_COMMAND_ENTIRE_DISP_ON)    //  Force whole LCD point

#define SET_BIAS_1() command_(NT7538_COMMAND_BIAS_1)  //  bias 1   1/7 bais
#define SET_BIAS_0() command_(NT7538_COMMAND_BIAS_0)  //  bias 0   1/9 bais

#define SET_MODIFY_READ() \
  command_(NT7538_COMMAND_RMW)  //  Stop automatic increment of the column address by the read instruction
#define RESET_MODIFY_READ() \
  command_(NT7538_COMMAND_RMW_END)              //  Cancel Modify_read, column address return to its
                                                //  initial value just before the Set Modify Read
                                                //  instruction is started
#define RESET() command_(NT7538_COMMAND_RESET)  //   system reset

#define SET_SHL_1() command_(NT7538_COMMAND_SHL_1)
#define SET_SHL_0() command_(NT7538_COMMAND_SHL_0)

#define OSC_31() command_(NT7538_COMMAND_OSC_31)  // 31.4kHz
#define OSC_26() command_(NT7538_COMMAND_OSC_26)  // 26.3kHz

// NT7538 COMMANDS
static const uint8_t NT7538_COMMAND_DISPLAY_OFF = 0xAE;
static const uint8_t NT7538_COMMAND_DISPLAY_ON = 0xAF;
static const uint8_t NT7538_COMMAND_ADC_NORMAL = 0xA0;
static const uint8_t NT7538_COMMAND_ADC_REVERSE = 0xA1;
static const uint8_t NT7538_COMMAND_BIAS_0 = 0xA2;
static const uint8_t NT7538_COMMAND_BIAS_1 = 0xA3;
static const uint8_t NT7538_COMMAND_DISP_NORMAL = 0xA6;
static const uint8_t NT7538_COMMAND_DISP_REVERSE = 0xA7;
static const uint8_t NT7538_COMMAND_ENTIRE_DISP_OFF = 0xA4;
static const uint8_t NT7538_COMMAND_ENTIRE_DISP_ON = 0xA5;
static const uint8_t NT7538_COMMAND_SHL_0 = 0xC0;  // SHL 0, COM0-COM63
static const uint8_t NT7538_COMMAND_SHL_1 = 0xC8;  // SHL 1, COM63-COM0

static const uint8_t NT7538_COMMAND_RMW = 0xE0;      // Read-Modify-Write
static const uint8_t NT7538_COMMAND_RMW_END = 0xEE;  // Read-Modify-Write End
static const uint8_t NT7538_COMMAND_RESET = 0xE2;

static const uint8_t NT7538_COMMAND_STATIC_INDICATOR_OFF = 0xAC;
static const uint8_t NT7538_COMMAND_STATIC_INDICATOR_ON = 0xAD;
static const uint8_t NT7538_COMMAND_OSC_31 = 0xE4;
static const uint8_t NT7538_COMMAND_OSC_26 = 0xE5;

int NT7538::get_width_internal() { return width_; }

int NT7538::get_height_internal() { return height_; }

float NT7538::get_setup_priority() const { return setup_priority::PROCESSOR; }

void NT7538::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NT7538...");
  this->spi_setup();

  this->dc_pin_->setup();  // OUTPUT
  this->cs_->setup();      // OUTPUT

  this->dc_pin_->digital_write(true);
  this->cs_->digital_write(true);

  backlight(false);

  this->init_reset_();
  delay(10);  // NOLINT

  ESP_LOGD(TAG, "  START");
  this->dump_config();
  ESP_LOGD(TAG, "  END");

  this->display_init_();

  this->init_internal_(this->get_buffer_length_());
  memset(this->buffer_, 0x00, this->get_buffer_length_());
}

void NT7538::backlight(bool onoff) {
  if (this->backlight_pin_ == nullptr)
    return;

  this->backlight_pin_->setup();
  this->backlight_pin_->digital_write(onoff);
}

// Display Start Line Set (0~63)
void NT7538::set_start_line(uint8_t line) {
  line |= 0x40;
  command_(line);
}

// Page Address Set (0~8)
void NT7538::set_page_address(uint8_t add) {
  add = 0xb0 | add;
  command_(add);
}

// Column Address Set (0~132)
void NT7538::set_column_address(uint8_t add) {
  command_((0x10 | (add >> 4)));  // high nibble
  command_((0x0f & add));         // low nibble
}

void HOT NT7538::data_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void HOT NT7538::command_(uint8_t value) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
  this->dc_pin_->digital_write(true);
}

// Write Display Data
void HOT NT7538::write_display_data() {
  // ESP_LOGW(TAG, "write_display_data");

  uint8_t n_cols = width_;        // 128 coloms
  uint8_t n_rows = height_ / 8u;  // 8 rows
  uint8_t i, j;

  // this->enable();

  for (i = 0; i < n_rows; i++) {  // 8
    set_page_address(i);
    set_column_address(0x00);
    for (j = 0; j < n_cols; j++) {
      data_(this->buffer_[j + (n_cols * i)]);
    }
  }
  // this->disable();
}

// Power Control: 4 (internal converter ON) + 2 (internal regulor ON) + 1 (internal follower ON)
void NT7538::power_control(uint8_t val) { command_((0x28 | val)); }

/**  Regulator resistor select (0~7)
 *
 *      1+Rb/Ra  Vo=(1+Rb/Ra)Vev    Vev=(1-(63-a)/162)Vref   2.1v
 *      0  3.0       4  5.0(default)
 *      1  3.5       5  5.5
 *      2  4         6  6
 *      3  4.5       7  6.4
 */
void NT7538::set_reg_resistor(uint8_t r) { command_((0x20 | r)); }

// The Electronic Volume (Contrast), (0~63), 32 default, Vev=(1-(63-a)/162)Vref   2.1v
void NT7538::set_contrast_control_register(uint8_t mod) {
  command_(0x81);
  command_(mod);
}

void NT7538::set_static_indicator(bool enabled, uint8_t mod) {
  if (enabled) {
    command_(NT7538_COMMAND_STATIC_INDICATOR_ON);
    command_(mod);
  } else {
    command_(NT7538_COMMAND_STATIC_INDICATOR_OFF);
    command_(0x00);
  }
}

void NT7538::update() {
  this->do_update_();
  this->write_display_data();
}

size_t NT7538::get_buffer_length_() {
  return size_t(width_ * height_) / 8u;
  // return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT NT7538::draw_absolute_pixel_internal(int x, int y, Color color) {
  // ESP_LOGW(TAG, "draw_absolute_pixel_internal: %dx%d (%d)", x, y, color.is_on());
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    ESP_LOGW(TAG, "Position out of area: %dx%d", x, y);
    return;
  }
  uint16_t pos = x + (y / 8) * this->get_width_internal();
  uint8_t subpos = y & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}

void NT7538::init_reset_() {
  if (this->reset_pin_ == nullptr)
    return;
    
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(true);
  delay(1);
  // Trigger Reset
  this->reset_pin_->digital_write(false);
  delay(10);
  // Wake up
  this->reset_pin_->digital_write(true);
}

//
void NT7538::display_chess(uint8_t value) {
  uint8_t i, j, k;
  for (i = 0; i < 0x08; i++) {
    set_page_address(i);
    set_column_address(0x00);
    for (j = 0; j < 0x10; j++) {
      for (k = 0; k < 0x04; k++)
        data_(value);
      for (k = 0; k < 0x04; k++)
        data_(~value);
    }
  }
}

//
void NT7538::display_picture(uint8_t pic[]) {
  uint8_t i, j;
  set_start_line(0x40);
  for (i = 0; i < 0x08; i++) {
    set_page_address(i);
    set_column_address(0x00);
    for (j = 0; j < 0x80; j++) {
      data_(pic[i * 0x80 + j]);
    }
  }
}

//
void NT7538::display_init_() {
  ESP_LOGD(TAG, "Initializing display...");

  DISPLAY_OFF();
  SET_ADC_REVERSE();
  REVERSE_DISPLAY_OFF();
  ENTIRE_DISPLAY_OFF();
  SET_BIAS_0();
  SET_SHL_0();
  SET_ADC_NORMAL();

  SET_SHL_1();
  SET_BIAS_0();

  power_control(0x07);                             // (0~7)
  set_reg_resistor(0x07);                          // (0~7)
  set_contrast_control_register(contrast_level_);  // (0~63)
  set_start_line(0x0);

  DISPLAY_ON();
  // this->write_display_data();
  display_chess(0xf);
}

void NT7538::dump_config() {
  LOG_DISPLAY("", "NT7538", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGD(TAG, "  Buffer Size: %zu", this->get_buffer_length_());
  ESP_LOGCONFIG(TAG, "  Height: %d", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %d", this->width_);
}

}  // namespace nt7538
}  // namespace esphome
