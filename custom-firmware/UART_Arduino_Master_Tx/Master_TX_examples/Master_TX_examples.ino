#include <Adafruit_GFX.h>

#define UART_CMD_SIZE 255 // min 50 - max 255
#define UART_DELAY_AFTER_CMD 4000+(UART_CMD_SIZE*170) // Each byte adds some delay before the next cmd is send

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 296
#define SCREEN_BUFFER_SIZE ((SCREEN_HEIGHT*SCREEN_WIDTH)/8)


GFXcanvas1 display(SCREEN_HEIGHT, SCREEN_WIDTH);
GFXcanvas1 display1(SCREEN_HEIGHT, SCREEN_WIDTH);

uint8_t seg_buffer[30] = {0b01010101, 0b01010101, 0b01010101, 0b01010101, 0b01010101};
uint8_t seg_buffer1[30] = {0b01010101, 0b01010101, 0b01010101, 0b01010101, 0b01010101};
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Hello");
  pinMode(LED_BUILTIN, OUTPUT);
  display.setRotation(1);
  display.setTextSize(2);
  display.setTextColor(1);
  display1.setRotation(1);
  display1.setTextSize(2);
  display1.setTextColor(1);

  display1.fillScreen(0);
  /* send_image(2, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    send_image(3, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    send_image(4, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    send_image(5, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    send_image(6, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    send_image(7, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, false);
    delay(20000);
    //  send_fw(0xffff, rawData, sizeof(rawData));
  */

  seg_buffer[0] = 0x00;
  seg_buffer[1] = 0x00;
  seg_buffer[2] = 0x00;

  seg_buffer[3] = 0x00;
  seg_buffer[4] = 0x00;
  seg_buffer[5] = 0x00;

  seg_buffer[6] = 0x00;
  seg_buffer[7] = 0x00;
  seg_buffer[8] = 0x00;

  seg_buffer[9]  = 0x00;
  seg_buffer[10]  = 0x00;
  seg_buffer[11] = 0x00;

  seg_buffer[12] = 0x00;
  seg_buffer[13] = 0x00;
  seg_buffer[14] = 0x44;

  seg_buffer[15] = 0x00;
  seg_buffer[16] = 0x00;
  seg_buffer[17] = 0x86;

  seg_buffer[18] = 0x00;
  seg_buffer[19] = 0x00;
  seg_buffer[20] = 0x04;

  seg_buffer[21] = 0x14;
  seg_buffer[22] = 0x25;
  seg_buffer[23] = 0x08;/*
    seg_buffer[21] = 0x08;
    seg_buffer[22] = 0x21;
    seg_buffer[23] = 0x08;*/

  seg_buffer[24] = 0x00;
  seg_buffer[25] = 0x20;
  seg_buffer[26] = 0xa0;

  seg_buffer[27] = 0x1c;
  seg_buffer[28] = 0x00;
  seg_buffer[29] = 0x80;
  send_segment_custom_lut_write_flash(0xffff, seg_buffer);
}

#define used_id 0xffff

#define CMD_TYPE_NODE_TX 0xAA
#define CMD_TYPE_STAY_AWAKE 0xAB

#define CMD_TYPE_REFRESH_SEG 0x41
#define CMD_TYPE_REFRESH_SEG_LOAD 0x42
#define CMD_TYPE_REFRESH_SEG_LOADED 0x43
#define CMD_TYPE_REFRESH_SEG_CUSTOM_LUT 0x45
#define CMD_TYPE_REFRESH_SEG_LOADED_CUSTOM_LUT 0x46
#define CMD_TYPE_REFRESH_PIXEL 0x47
#define CMD_TYPE_REFRESH_PIXEL_CUSTOM_LUT 0x48

#define CMD_TYPE_GET_SENSOR 0xF0
#define CMD_TYPE_PING 0xF1

#define CMD_TYPE_REFRESH_END_PARTIAL 0x13
#define CMD_TYPE_EPD_END_DATA_PARTIAL 0x14

#define CMD_TYPE_REFRESH 0x01
#define CMD_TYPE_REFRESH_INIT 0x02
#define CMD_TYPE_REFRESH_MID 0x03
#define CMD_TYPE_REFRESH_END 0x04
#define CMD_TYPE_SLEEP 0x05
#define CMD_TYPE_EPD_DATA 0x06
#define CMD_TYPE_EPD_INIT_MID_DATA 0x07
#define CMD_TYPE_EPD_INIT_DATA 0x08
#define CMD_TYPE_EPD_END_DATA 0x09

#define CMD_TYPE_ERASE_OTA_AREA 0x10
#define CMD_TYPE_WRITE_EEPROM 0x11
#define CMD_TYPE_DO_UPDATE 0x12

#define CMD_TYPE_WA 0xFF


uint16_t crc16(uint16_t cur_crc, uint8_t data)
{
  cur_crc ^= data;
  for (uint8_t i = 8; i > 0; i--)
  {
    if ((cur_crc & 0x001) != 0)
    {
      cur_crc >>= 1;
      cur_crc ^= 0x8005; // poly
    }
    else
    {
      cur_crc >>= 1;
    }
  }
  return cur_crc;
}

uint8_t out[UART_CMD_SIZE + 8];

void send_data(uint16_t endpoint, uint8_t cmd, uint8_t data_len, uint8_t *the_data)
{
  uint16_t crc_out = 0x0000;

  out[0] = 0xCA;
  out[1] = UART_CMD_SIZE;
  out[2] = data_len;
  out[3] = cmd;

  for (int i = 0; i < data_len; i++)
  {
    out[4 + i] = the_data[i];
  }

  for (int i = 0; i < (UART_CMD_SIZE - data_len); i++)
  {
    out[4 + data_len + i] = 0x00;
  }

  out[4 + UART_CMD_SIZE] = endpoint >> 8;
  out[5 + UART_CMD_SIZE] = endpoint & 0xff;

  for (int i = 0; i < (UART_CMD_SIZE + 6); i++) {
    crc_out += out[i];
  }
  out[6 + UART_CMD_SIZE] = crc_out >> 8;
  out[7 + UART_CMD_SIZE] = crc_out & 0xff;


  Serial1.write(0x00);
  Serial1.write(0x00);
  Serial1.write(0x00);
  for (int i = 0; i < (UART_CMD_SIZE + 8); i++) {
    Serial1.write(out[i]);
  }
}

void send_data(uint16_t endpoint, uint8_t cmd)
{
  send_data(endpoint, cmd, 0, NULL);
}

void send_segment(uint16_t endpoint, uint8_t *seg_buffer, boolean custom_lut)
{
  Serial.printf("Send segment to endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);

  if (custom_lut)
    send_data(endpoint, CMD_TYPE_REFRESH_SEG_CUSTOM_LUT, 30, seg_buffer);
  else
    send_data(endpoint, CMD_TYPE_REFRESH_SEG, 14, seg_buffer);

  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void send_segment_pixel(uint16_t endpoint, uint8_t *pixel_buffer)
{
  Serial.printf("Send segment to endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  //send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);

  send_data(endpoint, CMD_TYPE_REFRESH_PIXEL/*_CUSTOM_LUT*/, (endpoint / 8) + 1, pixel_buffer);

  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void send_segment_custom_lut_write_flash(uint16_t endpoint, uint8_t *lut_buffer)
{
  Serial.printf("Send LUT to endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);

  send_data(endpoint, CMD_TYPE_REFRESH_SEG_LOAD, 30, lut_buffer);

  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void send_image(uint16_t endpoint, uint8_t *img_buffer_black, uint8_t *img_buffer_red, uint16_t img_len, boolean do_refresh, boolean partial)
{
  uint16_t cur_posi = 0;

  uint16_t cur_len = img_len;

  Serial.printf("Send img: %d , Endp: %04X\r\n", img_len, endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);

  while (1) {
    cur_len = img_len - cur_posi;

    if (cur_len > UART_CMD_SIZE)
      cur_len = UART_CMD_SIZE;

    send_data(endpoint, (cur_posi == 0) ? CMD_TYPE_EPD_INIT_DATA : CMD_TYPE_EPD_DATA, cur_len, &img_buffer_black[cur_posi]);
    delayMicroseconds(UART_DELAY_AFTER_CMD);

    cur_posi += cur_len;

    if (cur_posi >= img_len)
    {
      break;
    }
  }
  cur_posi = 0;
  cur_len = img_len;

  while (1) {
    cur_len = img_len - cur_posi;

    if (cur_len > UART_CMD_SIZE)
      cur_len = UART_CMD_SIZE;

    send_data(endpoint, (cur_posi == 0) ? CMD_TYPE_EPD_INIT_MID_DATA : CMD_TYPE_EPD_DATA, cur_len, &img_buffer_red[cur_posi]);

    delayMicroseconds(UART_DELAY_AFTER_CMD);
    cur_posi += cur_len;

    if (cur_posi >= img_len)
    {
      break;
    }
  }
  if (do_refresh)
    send_data(endpoint, partial ? CMD_TYPE_REFRESH_END_PARTIAL : CMD_TYPE_REFRESH_END);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void send_fw(uint16_t endpoint, uint8_t *fw_buffer, uint16_t fw_len)
{
  uint16_t cur_posi = 0;
  uint16_t cur_len = fw_len;
  uint16_t fw_crc = 0x0000;

  for (int i = 0; i < fw_len; i++) {
    fw_crc = crc16(fw_crc, fw_buffer[i]);
  }

  Serial.printf("Send FW: %d , Endp: %04X CRC: %04X\r\n", fw_len, endpoint, fw_crc);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_ERASE_OTA_AREA);
  delay(600);

  while (1) {

    cur_len = fw_len - cur_posi;
    if (cur_len > UART_CMD_SIZE)
      cur_len = UART_CMD_SIZE;

    send_data(endpoint, CMD_TYPE_WRITE_EEPROM, cur_len, &fw_buffer[cur_posi]);

    delayMicroseconds(UART_DELAY_AFTER_CMD);
    cur_posi += cur_len;

    if (cur_posi >= fw_len)
    {
      break;
    }
  }

  uint8_t update_go_buffer[] = {fw_crc >> 8, fw_crc & 0xff, fw_len >> 8, fw_len & 0xff};
  send_data(endpoint, CMD_TYPE_DO_UPDATE, sizeof(update_go_buffer), update_go_buffer);
  delay(8000);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void set_stay_awake(uint16_t endpoint, uint8_t seconds_div10)
{
  Serial.printf("Set stay awake Endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_STAY_AWAKE, 1, (uint8_t *)&seconds_div10);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void get_sensor(uint16_t endpoint)
{
  Serial.printf("GET Data Endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_GET_SENSOR);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_PING);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void just_refresh(uint16_t endpoint)
{
  uint8_t ref_data[20] = {0x12, 0x34, 0x45, 0x45, 0x78};
  Serial.printf("GET Data Endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_REFRESH, sizeof(ref_data), ref_data);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void just_refresh(uint16_t endpoint, uint8_t a, uint8_t b, uint8_t part)
{
  uint8_t ref_data[20] = {a, b, part, 0x45, 0x78};
  Serial.printf("GET Data Endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, CMD_TYPE_REFRESH, sizeof(ref_data), ref_data);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void just_refresh_end(uint16_t endpoint, boolean partial)
{
  Serial.printf("GET Data Endp: %04X\r\n", endpoint);
  Serial1.begin(115200, SERIAL_8N1, 4, 2);
  Serial1.write(0x00);
  send_data(endpoint, CMD_TYPE_WA);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  send_data(endpoint, partial ? CMD_TYPE_REFRESH_END_PARTIAL : CMD_TYPE_REFRESH_END);
  delayMicroseconds(UART_DELAY_AFTER_CMD);
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

uint8_t df = 0;
uint8_t fd = 0;

int posi = 60;

void loop()
{

  Serial.println("Start");
  // get_sensor(1);

  // just_refresh(0xffff);
  // just_refresh(0xffff, df++, fd, 1);
  ////

  /*
    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 0 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(0, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, true, true);
  */
  /*
    seg_buffer[0] ++;
    seg_buffer[1] ++;
    seg_buffer[2] ++;

    seg_buffer[3] ++;
    seg_buffer[4] ++;
    seg_buffer[5] ++;

    seg_buffer[6] ++;
    seg_buffer[7] ++;
    seg_buffer[8] ++;

    seg_buffer[9] ++;
    seg_buffer[10] ++;
    seg_buffer[11] ++;

    seg_buffer[12] = 0x00;
    seg_buffer[13] = 0x00;
    seg_buffer[14] = 0x44;

    seg_buffer[15] = 0x00;
    seg_buffer[16] = 0x00;
    seg_buffer[17] = 0x86;

    seg_buffer[18] = 0x00;
    seg_buffer[19] = 0x00;
    seg_buffer[20] = 0x04;

    seg_buffer[21] = 0x14;
    seg_buffer[22] = 0x25;
    seg_buffer[23] = 0x08;

    seg_buffer[24] = 0x00;
    seg_buffer[25] = 0x20;
    seg_buffer[26] = 0xa0;

    seg_buffer[27] = 0x1c;
    seg_buffer[28] = 0x00;
    seg_buffer[29] = 0x80;*/

  //send_segment(1, seg_buffer, false);
  //set_stay_awake(0xffff, 30);
  send_segment_pixel(27, seg_buffer1);
  delay(5000);
  send_segment_pixel(28, seg_buffer1);
  /*
    display.fillScreen(0);
    display.setRotation(3);
    display.setCursor(10, posi);
    display.println("Display 1 Different");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(3);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(2, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(3);
    display.setCursor(10, posi);
    display.println("Display 2 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(3);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(3, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(3);
    display.setCursor(10, posi);
    display.println("Display 3 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(3);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(4, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, posi);
    display.println("Display 4 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(5, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, posi);
    display.println("Display 5 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(6, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, posi);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(7, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);*/
  //////////////////////////////////////////

  /*
    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, posi);
    display.println("Display Testing");
    display.println("  Millis: "  + String(millis()));
    display1.fillScreen(0);
    send_image(2, (uint8_t *)myBitmap1, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);
    send_image(3, (uint8_t *)myBitmap2, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);
    send_image(4, (uint8_t *)myBitmap4, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);
    send_image(5, (uint8_t *)myBitmap5, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);
    send_image(6, (uint8_t *)myBitmap7, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);
    send_image(7, (uint8_t *)myBitmap8, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, false);

    just_refresh_end(0xffff, false);
    delay(24000);
    send_image(7, (uint8_t *)myBitmapabcd000000000023, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    send_image(6, (uint8_t *)myBitmapffff02016ea23802, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    send_image(5, (uint8_t *)myBitmapabcd000000000035, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    send_image(4, (uint8_t *)myBitmapabcd00000000002d, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    send_image(3, (uint8_t *)myBitmapabcd000000000004, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    send_image(2, (uint8_t *)myBitmapabcd00000000000c, display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);*/
  /* display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(8, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(9, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(10, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(11, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(12, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(13, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(14, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(15, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(16, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);

    display.fillScreen(0);
    display.setRotation(1);
    display.setCursor(10, 60);
    display.println("Display 6 Testing");
    display.println("  Millis: "  + String(millis()));

    display1.fillScreen(0);
    display1.setRotation(1);
    display1.fillCircle(30, 30, 14, 1);
    display1.fillCircle(80, 30, 14, 1);
    send_image(17, display.getBuffer(), display1.getBuffer(), SCREEN_BUFFER_SIZE, false, true);
    //////////////////////////////////////////////
  */

  if (posi == 50)
    posi = 80;
  else
    posi = 50;
  //just_refresh_end(0xffff, true);
  delay(5000);
  digitalWrite(LED_BUILTIN, HIGH);

}
