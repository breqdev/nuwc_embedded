#include <string>

#include <Arduino.h>
#include <Wire.h>
#include <KeyboardBT.h>

#include "lvgl.h"
#include "ssd1306.h"

LV_FONT_DECLARE(noto_emoji)

const int I2C_SDA = 16;
const int I2C_SCL = 17;

const int ENCODER_BUTTON = 18;
const int ENCODER_CLOCK = 14;
const int ENCODER_DATA = 15;
long int encoder_last_tick = 0;
int encoder_ticks = 0;

static const uint16_t WIDTH = 128;
static const uint16_t HEIGHT = 64;
static const uint8_t I2C_ADDR = 0x3C;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[WIDTH * HEIGHT / 8];

const char *emojis[] = { "😀", "🤣", "😍", "🤔", "👍", "👎", "👋", "📻", "📡", "⚡" };

void ssd1306_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint8_t start_page = area->y1 / 8;
  uint8_t end_page = area->y2 / 8;
  uint8_t start_col = area->x1;
  uint8_t end_col = area->x2;

  uint8_t *color_buffer = reinterpret_cast<uint8_t *>(color_p);

  uint8_t commands[] = {
    SSD1306_PAGEADDR,
    start_page,
    end_page,
    SSD1306_COLUMNADDR,
    start_col,
    end_col,
  };

  ssd1306_command(I2C_ADDR, commands, sizeof(commands));

  for (uint8_t page = 0; page + start_page <= end_page; ++page) {
    uint16_t buffer_index = page * (end_col - start_col + 1);
    uint8_t *data = color_buffer + buffer_index;
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0x40);
    for (uint8_t col = start_col; col <= end_col; ++col) {
      // TODO: send this byte of image data to the display
      Wire.write(data[col]);

      if ((col - start_col) % 32 == 0) {
        Wire.endTransmission();
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0x40);
      }
    }
    Wire.endTransmission();
  }

  lv_disp_flush_ready(disp_drv);
}

void ssd1306_round_to_page_boundary(lv_disp_drv_t *disp_drv, lv_area_t *area) {
  area->y1 = area->y1 & ~0x07;
  area->y2 = (area->y2 & ~0x07) + 7;

  area->x1 = 0;
  area->x2 = WIDTH - 1;
}

void ssd1306_set_px(lv_disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa) {
  buf += buf_w * (y >> 3) + x;
  if (lv_color_brightness(color) > 128) (*buf) |= (1 << (y % 8));
  else (*buf) &= ~(1 << (y % 8));
}

void encoder_isr() {
  bool data = digitalRead(ENCODER_DATA);

  // Sometimes the switch is "bouncy," meaning that a single click can create
  // a few electrical connections in rapid succession as the contacts in the
  // switch bounce off of each other. To compensate for this, we ignore any
  // ticks that happened within 75 milliseconds of each other.
  long int now = millis();
  if (now - encoder_last_tick < 75) {
    return;
  }
  encoder_last_tick = now;

  // Hint: The encoder_ticks variable stores the number of ticks the encoder
  // has moved from its current position. You'll need to update it here based
  // on the direction of the encoder tick.
  if (data) {
    encoder_ticks += 1;
  } else {
    encoder_ticks -= 1;
  }
}

void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  data->enc_diff = encoder_ticks;
  data->state = digitalRead(ENCODER_BUTTON) ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
  encoder_ticks = 0;
}

lv_obj_t *msg_label;
lv_obj_t *author_label;


static void emoji_send(lv_event_t *e) {
  lv_obj_t *button = lv_event_get_current_target(e);
  int emoji_idx = (int)lv_obj_get_user_data(button);
  lv_label_set_text(msg_label, emojis[emoji_chr]);
  lv_label_set_text(author_label, "Sent:");

  uint8_t emoji_chr = 225 + (uint8_t)emoji_idx;

  // TODO: send the emoji character
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // TODO: initialize the keyboard

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  attachInterrupt(ENCODER_CLOCK, encoder_isr, FALLING);
  pinMode(ENCODER_CLOCK, INPUT_PULLUP);
  pinMode(ENCODER_DATA, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);

  pinMode(I2C_SDA, OUTPUT);
  pinMode(I2C_SCL, OUTPUT);
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  ssd1306_init(I2C_ADDR, WIDTH, HEIGHT);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, WIDTH * HEIGHT);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = WIDTH;
  disp_drv.ver_res = HEIGHT;
  disp_drv.flush_cb = ssd1306_flush;
  disp_drv.rounder_cb = ssd1306_round_to_page_boundary;
  disp_drv.set_px_cb = ssd1306_set_px;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_ENCODER;
  indev_drv.read_cb = encoder_read;
  lv_indev_t *enc_indev = lv_indev_drv_register(&indev_drv);

  static lv_style_t button_focused_style;
  lv_style_set_bg_color(&button_focused_style, lv_color_hex(0x000000));
  lv_style_set_text_color(&button_focused_style, lv_color_hex(0xFFFFFF));
  lv_style_set_bg_opa(&button_focused_style, LV_OPA_COVER);

  static lv_style_t emoji_style;
  lv_style_set_text_font(&emoji_style, &noto_emoji);

  lv_obj_t *screen = lv_obj_create(lv_scr_act());
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *emoji_row = lv_obj_create(screen);
  lv_obj_set_size(emoji_row, WIDTH, HEIGHT / 2);
  lv_obj_set_flex_flow(emoji_row, LV_FLEX_FLOW_ROW);

  lv_group_t *g = lv_group_create();
  for (int i = 0; i < sizeof(emojis) / sizeof(emojis[0]); ++i) {
    lv_obj_t *btn = lv_btn_create(emoji_row);
    lv_obj_add_style(btn, &button_focused_style, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(btn, emoji_send, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(btn, (void *)i);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_add_style(label, &emoji_style, LV_PART_MAIN);
    lv_label_set_text(label, emojis[i]);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_group_add_obj(g, btn);
  }
  lv_indev_set_group(enc_indev, g);

  lv_obj_t *msg_row = lv_obj_create(screen);
  lv_obj_set_size(msg_row, WIDTH, HEIGHT / 2);
  lv_obj_set_flex_flow(msg_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(msg_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

  author_label = lv_label_create(msg_row);

  static lv_style_t monospace;
  lv_style_set_text_font(&monospace, &lv_font_unscii_16);

  lv_obj_add_style(author_label, &monospace, LV_PART_MAIN);
  lv_label_set_text(author_label, "");

  msg_label = lv_label_create(msg_row);
  lv_obj_add_style(msg_label, &emoji_style, LV_PART_MAIN);
  lv_label_set_text(msg_label, "");
}

void loop() {
  lv_timer_handler();
  delay(5);
}
