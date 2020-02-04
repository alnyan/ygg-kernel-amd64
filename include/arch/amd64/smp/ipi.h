#pragma once
#include "sys/types.h"

#define IPI_VECTOR_GENERIC      0xF0
#define IPI_VECTOR_PANIC        0xF3

void amd64_ipi_send(int cpu, uint8_t vector);
