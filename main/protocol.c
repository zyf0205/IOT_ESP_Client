#include "protocol.h"
#include "sensor.h"

// CRC16(Modbus)
uint16_t calc_crc16(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i]; /*异或运算*/
    for (int j = 0; j < 8; j++)
    {
      /*二进制右移异或，模拟除法取余。*/
      if ((crc & 1) != 0) /*是1异或掉*/
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else /*是0不用管*/
      {
        crc >>= 1;
      }
    }
  }
  return crc;
}

/*小端：低位字节排在内存的低地址端*/
void write_u16_le(uint8_t *buf, uint16_t val)
{
  buf[0] = val & 0xFF;        /*得到低八位首地址*/
  buf[1] = (val >> 8) & 0xFF; /*得到高八位首地址*/
}

/*同上，针对32位无符号整数*/
void write_u32_le(uint8_t *buf, uint32_t val)
{
  buf[0] = val & 0xFF;
  buf[1] = (val >> 8) & 0xFF;
  buf[2] = (val >> 16) & 0xFF;
  buf[3] = (val >> 24) & 0xFF;
}