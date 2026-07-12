#include <systemc>
#include <vector>
#include <cstdint>
#include "../include/run_simulation.hpp"

int main()
{
  // Test config
  const uint32_t cycles = 100;
  const char *tracefile = "error_test_trace";
  const uint8_t endianness = 0; // can be 0 or 1 — doesn't matter for parity test
  const uint32_t latencyScrambling = 2;
  const uint32_t latencyEncrypt = 3;
  const uint32_t latencyMemoryAccess = 4;
  const uint32_t seed = 42;

  std::vector<Request> requests = {
      // Step 1: Write value to memory
      {0x1000, 0xCAFEBABE, 0, 1, UINT32_MAX, 0},

      // Step 2: Inject a fault at that address, flip bit 0 (or any bit)
      {0, 0, 0, 0, 0x4099A181, 0},

      // Step 3: Read back → should detect parity error
      {0x1000, 0, 1, 0, UINT32_MAX, 0}};

  run_simulation(
      cycles,
      tracefile,
      endianness,
      latencyScrambling,
      latencyEncrypt,
      latencyMemoryAccess,
      seed,
      requests.size(),
      requests.data());

  return 0;
}
