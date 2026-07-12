#ifndef RUN_H
#define RUN_H

#include "relevant_structs.h"

#ifdef __cplusplus
extern "C"
{
#endif

  struct IOTiming
  {
    uint32_t encryption_cycles;
    uint32_t scrambling_cycles;
    uint32_t memory_cycles;
  };

  struct Result run_simulation(
      uint32_t cycles,
      const char *tracefile,
      uint8_t endianness,
      uint32_t latencyScrambling,
      uint32_t latencyEncrypt,
      uint32_t latencyMemoryAccess,
      uint32_t seed,
      uint32_t numRequests,
      struct Request *requests);

#ifdef __cplusplus
}
#endif

#endif // RUN_H