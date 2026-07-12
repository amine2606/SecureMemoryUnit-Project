#include "../include/run_simulation.hpp"
#include "../include/secure_memory_unit.hpp"
#include <systemc>
#include <fstream>
#include <iomanip>
SC_MODULE(SimulationTop)
{
  sc_core::sc_clock clk{"clk", 1, SC_NS};
  sc_core::sc_signal<sc_dt::sc_uint<32>> addr, wdata, fault;
  sc_core::sc_signal<bool> r, w;
  sc_core::sc_signal<sc_dt::sc_bv<4>> faultBit;
  sc_core::sc_signal<sc_dt::sc_uint<32>> rdata;
  sc_core::sc_signal<bool> ready, error;

  SECURE_MEMORY_UNIT smu{"smu"};

  SC_CTOR(SimulationTop) : smu("smu")
  {
    smu.clk(clk);
    smu.addr(addr);
    smu.wdata(wdata);
    smu.r(r);
    smu.w(w);
    smu.fault(fault);
    smu.faultBit(faultBit);
    smu.rdata(rdata);
    smu.ready(ready);
    smu.error(error);
  }
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
    struct Request *requests)
{
  // Initialize SystemC kernel
  sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY);

  // Create simulation top
  SimulationTop top{"top"};
  SECURE_MEMORY_UNIT &smu = top.smu;

  // Configure SMU
  smu.setSeed(seed);
  smu.endianness = endianness;
  smu.latencyScrambling = latencyScrambling;
  smu.latencyEncrypt = latencyEncrypt;
  smu.latencyMemoryAccess = latencyMemoryAccess;

  Result result = {};
  IOTiming timing = {};
  sc_trace_file *tf = nullptr;
  if (tracefile)
  {
    tf = sc_create_vcd_trace_file(tracefile);
    if (!tf)
    {
      std::cerr << "Error: Unable to create trace file" << std::endl;
    }
    else
    {
      std::cout << "Trace file created successfully" << std::endl;
      // Trace all signals
      sc_trace(tf, top.clk, "clk");
      sc_trace(tf, top.addr, "addr_signal");
      sc_trace(tf, top.wdata, "wdata_signal");
      sc_trace(tf, top.fault, "fault_signal");
      sc_trace(tf, top.r, "read_signal");
      sc_trace(tf, top.w, "write_signal");
      sc_trace(tf, top.faultBit, "fault_Bit");
      sc_trace(tf, top.rdata, "read_data");
      sc_trace(tf, top.error, "error_signal");
      sc_trace(tf, top.ready, "ready_signal");
    }
  }

  // Initialize signals
  top.r.write(false);
  top.w.write(false);
  top.fault.write(UINT32_MAX);
  sc_core::sc_start(1, SC_NS); // Initial cycle

  // Process requests
  for (uint32_t i = 0; i < numRequests && result.cycles < cycles; i++)
  {

    struct Request req = requests[i];

    // Setup request
    top.addr.write(req.addr);
    top.wdata.write(req.data);
    top.fault.write(req.fault);
    top.faultBit.write(sc_dt::sc_bv<4>(req.faultBit));
    top.r.write(req.r);
    top.w.write(req.w);
    smu.error.write(false);

    // Count operation type
    if (req.r && !req.w)
      result.reads++;
    else if (!req.r && req.w)
      result.writes++;

    if (req.fault != UINT32_MAX)
      result.faults++;

    // Process request
    do
    {
      sc_core::sc_start(1, SC_NS);
      result.cycles++;
    } while (!smu.ready_value && result.cycles < cycles);

    // Check for errors
    if (top.error.read())
    {
      break;
      result.errors++;
    }

    // Update timing
    if (req.w)
    {
      timing.encryption_cycles += std::max(latencyEncrypt, latencyScrambling);
      timing.memory_cycles += latencyMemoryAccess;
      timing.scrambling_cycles += latencyScrambling;
    }
    else if (req.r)
    {
      timing.scrambling_cycles += latencyScrambling;
      timing.encryption_cycles += latencyEncrypt;
      timing.memory_cycles += latencyMemoryAccess;
    }
  }

  // Print results
  std::cout << "\n=== Secure Memory I/O Analysis ===\n";
  std::cout << "Total cycles: " << result.cycles << "\n";
  std::cout << "Operations completed: " << result.reads + result.writes + result.faults << "\n";
  std::cout << "  Reads: " << result.reads << "\n";
  std::cout << "  Writes: " << result.writes << "\n";
  std::cout << "Total faults injected: " << result.faults << "\n";
  std::cout << "Errors detected: " << result.errors << "\n\n";

  // Calculate percentages
  uint32_t total_measured = timing.encryption_cycles +
                            timing.scrambling_cycles +
                            timing.memory_cycles;

  if (total_measured > 0)
  {
    std::cout << "Latency Distribution:\n";
    std::cout << "  Encryption: " << timing.encryption_cycles
              << " (" << (timing.encryption_cycles * 100 / total_measured) << "%)\n";
    std::cout << "  Scrambling: " << timing.scrambling_cycles
              << " (" << (timing.scrambling_cycles * 100 / total_measured) << "%)\n";
    std::cout << "  Memory Access: " << timing.memory_cycles
              << " (" << (timing.memory_cycles * 100 / total_measured) << "%)\n";
  }

  // Close trace file
  if (tf)
  {
    sc_core::sc_close_vcd_trace_file(tf);
  }

  return result;
}

int sc_main(int argc, char *argv[])
{
  return 0;
}