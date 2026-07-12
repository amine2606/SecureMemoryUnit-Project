#ifndef RELEVANT_STRUCTS_H
#define RELEVANT_STRUCTS_H

#include <stdint.h>

// I/O-centric results (no gate counts)
struct Result
{
  uint32_t cycles; // Total simulation cycles
  uint32_t errors; // Parity errors detected
  uint32_t reads;  // Successful read ops
  uint32_t writes; // Successful write ops
  uint32_t faults; // Injected faults
};

struct Request
{
  uint32_t addr;
  uint32_t data;
  uint8_t r;
  uint8_t w;
  uint32_t fault;
  uint8_t faultBit;
};

#endif