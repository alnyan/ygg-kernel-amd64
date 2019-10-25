#pragma once
#include <stdint.h>

#define RS232_COM1  0x3F8

void rs232_init(uint16_t port);
void rs232_send(uint16_t port, char c);
char rs232_recv(uint16_t port);
