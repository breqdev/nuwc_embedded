#include "Wire.h"

#include "ssd1306.h"

void ssd1306_command(uint8_t i2c_addr, uint8_t* commands, uint8_t num) {
  Wire.beginTransmission(i2c_addr);
  Wire.write(0x00);
  while (num--) {
    Wire.write(*(commands++));
  }
  Wire.endTransmission();
}

void ssd1306_init(uint8_t i2c_addr, uint16_t width, uint16_t height) {
  Wire.setClock(400000);

  uint8_t startup1[] = {
    SSD1306_DISPLAYOFF,
    SSD1306_SETDISPLAYCLOCKDIV,
    0x80,
    SSD1306_SETMULTIPLEX,
    (uint8_t)(height - 1),

  };
  ssd1306_command(i2c_addr, startup1, sizeof(startup1));

  uint8_t startup2[] = {
    SSD1306_SETDISPLAYOFFSET,
    0x00,
    SSD1306_SETSTARTLINE | 0x0,
    SSD1306_CHARGEPUMP,
    0x14,
  };
  ssd1306_command(i2c_addr, startup2, sizeof(startup2));

  uint8_t startup3[] = {
    SSD1306_MEMORYMODE,
    0x00,
    SSD1306_SEGREMAP | 0x1,
    SSD1306_COMSCANDEC,
  };
  ssd1306_command(i2c_addr, startup3, sizeof(startup3));

  uint8_t startup4[] = {
    SSD1306_SETCOMPINS,
    0x12,
    SSD1306_SETCONTRAST,
    0xCF,
  };
  ssd1306_command(i2c_addr, startup4, sizeof(startup4));

  uint8_t startup5[] = {
    SSD1306_SETPRECHARGE,
    0x22,
    SSD1306_SETVCOMDETECT,
    0x20,
  };
  ssd1306_command(i2c_addr, startup5, sizeof(startup5));

  uint8_t startup6[] = {
    SSD1306_DISPLAYALLON_RESUME,
    SSD1306_INVERTDISPLAY,
    SSD1306_DEACTIVATE_SCROLL,
    SSD1306_DISPLAYON
  };
  ssd1306_command(i2c_addr, startup6, sizeof(startup6));
}
