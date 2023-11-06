#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>

#include "lvgl.h"
#include "ssd1306.h"

const int I2C_SDA = 16;
const int I2C_SCL = 17;

const int ENCODER_BUTTON = 18;
const int ENCODER_DATA = 14;
const int ENCODER_CLOCK = 15;
long int encoder_last_tick = 0;
int encoder_ticks = 0;

const int SERVO_PIN = 0;
Servo myservo;

static const uint16_t WIDTH = 128;
static const uint16_t HEIGHT = 64;
static const uint8_t I2C_ADDR = 0x3C;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[WIDTH * HEIGHT];

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
    for (uint8_t col = start_col; col <= end_col; ++col) {
      // TODO: send this byte of image data to the display
      uint8_t data_byte = data[col];

      Wire.beginTransmission(I2C_ADDR);
      // two things are missing here...
      Wire.write(0x40);
      Wire.write(data_byte);
      Wire.endTransmission();
    }
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
    // TODO
  } else {
    encoder_ticks -= 1;
    // TODO
  }
}

void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  data->enc_diff = encoder_ticks;
  data->state = digitalRead(ENCODER_BUTTON) ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
  encoder_ticks = 0;
}

int servo_position = 45;
lv_obj_t *servo_label;

static void button1_callback(lv_event_t * e)
{
  // Right now, this just reduces the angle
  servo_position -= 15;
  if (servo_position < 0) {
    servo_position = 0;
  }
  // TODO: make it always reset to 0 instead
  servo_position = 0;
  myservo.write(servo_position);
  lv_label_set_text_fmt(servo_label, "%d°", servo_position);

}

static void button2_callback(lv_event_t * e)
{
  servo_position += 15;
  if (servo_position > 90) {
    servo_position = 90;
  }
  myservo.write(servo_position);
  lv_label_set_text_fmt(servo_label, "%d°", servo_position);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  attachInterrupt(ENCODER_CLOCK, encoder_isr, FALLING);
  pinMode(ENCODER_CLOCK, INPUT_PULLUP);
  pinMode(ENCODER_DATA, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);

  myservo.attach(0);
  myservo.write(servo_position);

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
  lv_indev_t* enc_indev = lv_indev_drv_register(&indev_drv);

  // lv_obj_t* hello_label = lv_label_create(lv_scr_act());
  // lv_label_set_text(hello_label, "Hello from NUWC");
  // lv_obj_align(hello_label, LV_ALIGN_CENTER, 0, 0);

  // Uncomment the following to show the buttons:

  lv_obj_t * cont_row = lv_obj_create(lv_scr_act());
  lv_obj_set_flex_flow(cont_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cont_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

  lv_obj_t *btn = lv_btn_create(cont_row);
  lv_obj_t *label = lv_label_create(btn);
  lv_obj_add_event_cb(btn, button1_callback, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(label, "0");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *btn2 = lv_btn_create(cont_row);
  lv_obj_t *label2 = lv_label_create(btn2);
  lv_obj_add_event_cb(btn2, button2_callback, LV_EVENT_CLICKED, NULL);
  lv_label_set_text(label2, "+");
  lv_obj_align(label2, LV_ALIGN_CENTER, 0, 0);

  servo_label = lv_label_create(cont_row);
  lv_label_set_text_fmt(servo_label, "%d°", servo_position);

  lv_group_t *g = lv_group_create();
  lv_group_add_obj(g, btn);
  lv_group_add_obj(g, btn2);
  lv_indev_set_group(enc_indev, g);
}

void loop() {
  lv_timer_handler();
  delay(5);
}
