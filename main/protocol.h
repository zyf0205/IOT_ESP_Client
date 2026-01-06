#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// ================= 协议常量 =================
#define FRAME_HEAD 0xAA55        /*起始信号*/
#define FRAME_VER 0x12           /*版本*/
#define CMD_HEARTBEAT 0x01       /*设备心跳*/
#define CMD_HEARTBEAT_RESP 0x02  /*心跳响应*/
#define CMD_REGISTER 0x03        /*设备注册*/
#define CMD_REGISTER_ANSWER 0x04 /*注册响应*/
#define CMD_REPORT 0x10          /*传感器数据上报*/
#define CMD_ACK 0x20             /*确认上报*/
#define CMD_FAULT 0x30           /*故障通知*/
#define CMD_CONTROL 0x80         /*控制*/

// ================= 协议结构 =================
typedef struct
{
  uint16_t frame_head;
  uint8_t frame_ver;
  uint8_t cmd;
  uint8_t seq;
  uint32_t dev_id;
  uint16_t payload_len;
  uint8_t *payload;
  uint16_t crc;
} protocol_frame_t;

// ================= 函数声明 =================
uint16_t calc_crc16(const uint8_t *data, size_t len);
void write_u16_le(uint8_t *buf, uint16_t val);
void write_u32_le(uint8_t *buf, uint32_t val);

#endif // PROTOCOL_H