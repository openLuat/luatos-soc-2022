#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint16_t crc16ccitt(uint16_t crc, uint8_t *bytes, int start, int len)
{
    for (; start < len; start++) {
        crc = ((crc >> 8) | (crc << 8)) & 0xffff;
        crc ^= (bytes[start] & 0xff);// byte to int, trunc sign
        crc ^= ((crc & 0xff) >> 4);
        crc ^= (crc << 12) & 0xffff;
        crc ^= ((crc & 0xFF) << 5) & 0xffff;
    }
    crc &= 0xffff;
    return crc;
}
