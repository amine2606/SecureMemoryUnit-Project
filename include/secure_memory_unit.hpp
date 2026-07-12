#ifndef SECURE_MEMORY_UNIT_HPP
#define SECURE_MEMORY_UNIT_HPP

#include <systemc.h>
#include <map>
#include "secure.hpp"
#include "parity.hpp"

SC_MODULE(SECURE_MEMORY_UNIT)
{
  // got inspired a lot from T8-2(refrenced here as TA) main memory

  // Ports :in
  sc_in<bool> clk;
  sc_in<sc_uint<32>> addr;
  sc_in<sc_uint<32>> wdata;
  sc_in<bool> r, w;
  sc_in<sc_uint<32>> fault;
  sc_in<sc_bv<4>> faultBit;

  // Ports :out
  sc_out<sc_uint<32>> rdata;
  sc_out<bool> ready;
  sc_out<bool> error;

  // Submodules
  PARITY parity;
  SECURE secure;

  // Internal signals for the submodules
  sc_signal<bool> start_read_write, start_scramble, write_en, read_en, start_parity;
  sc_signal<sc_uint<8>> data_in_signal_parity;
  sc_signal<sc_uint<32>> phys_addr;
  sc_signal<bool> parity_error;

  sc_vector<sc_signal<sc_uint<8>>> to_decrypt{"to_decrypt", 4};
  sc_vector<sc_signal<sc_uint<8>>> encrypted_output{"encrypted_output", 4};
  sc_vector<sc_signal<sc_uint<8>>> decrypted_output{"decrypted_output", 4};
  sc_vector<sc_signal<sc_uint<32>>> scrambled_addrs{"scrambled_addrs", 4};

  // map to store memory and the correspending addresses : inspired from the TA 8-2 main memory
  std::map<sc_uint<32>, sc_uint<8>> memory;
  // options
  uint32_t seed = 0;
  uint8_t endianness = 0;
  uint32_t latencyScrambling = 0;
  uint32_t latencyEncrypt = 0;
  uint32_t latencyMemoryAccess = 0;
  // help variable to know if we're currently ready to start an operation
  bool ready_value = true;

  SC_CTOR(SECURE_MEMORY_UNIT) : parity("parity"), secure("secure")
  {
    // Bind SECURE
    secure.start_read_write.bind(start_read_write);
    secure.start_scramble.bind(start_scramble);
    secure.w_oder_r.bind(write_en);
    secure.addr.bind(addr);
    secure.wdata.bind(wdata);

    for (int i = 0; i < 4; i++)
    {
      secure.to_decrypt[i].bind(to_decrypt[i]);
      secure.scrambled_addrs[i].bind(scrambled_addrs[i]);
      secure.encrypted_output[i].bind(encrypted_output[i]);
      secure.decrypted_output[i].bind(decrypted_output[i]);
    }

    // Bind PARITY
    parity.data_in.bind(data_in_signal_parity);
    parity.physical_addr.bind(phys_addr);
    parity.write_enable.bind(write_en);
    parity.read_enable.bind(read_en);
    parity.start.bind(start_parity);
    parity.parity_error.bind(parity_error);

    SC_CTHREAD(behavior, clk.pos());
  }

  void behavior()
  {
    std::cout << "\n[INFO] === SecureMemoryUnit started ===\n";
    std::cout << "[CONFIG] Endianness: " << (int)endianness << " (0=Little, 1=Big)\n";
    std::cout << "[CONFIG] Seed: " << seed << "\n";
    std::cout << "[CONFIG] Latency - Scrambling: " << latencyScrambling
              << ", Encryption: " << latencyEncrypt
              << ", MemoryAccess: " << latencyMemoryAccess << "\n";

    while (true)
    {
      wait();
      std::cout << "\n[Cycle " << sc_time_stamp() << "]\n";
      std::cout << "[DEBUG] clk ↑, r=" << r.read() << ", w=" << w.read() << ", addr="
                << addr.read() << ", wdata=" << wdata.read() << "\n";
      // in evry clock cycle check if the value in fault and inject a fault if it's not UINT32 MAX
      if (fault.read() != UINT32_MAX && ready_value)
      {
        fault_injection();
      }
      // the secure memory unit is not ready for a new operation: we are currently processing another op
      if (!ready_value)
        continue;

      // no read and write at the same time -> check that only one of them is set to be true
      if (w.read() && !r.read())
      {
        std::cout << "[ACTION] Write triggered\n";
        // write operation-> set ready to be false and process the write operation
        ready.write(false);
        ready_value = false;
        write_process();
      }
      else if (r.read() && !w.read())
      {
        std::cout << "[ACTION] Read triggered\n";
        // read operation-> set ready to be false and process the read operation
        ready.write(false);
        ready_value = false;
        read_process();
      }
      // no current operation or both asked at the same time(cannot happen) -> set ready to be true
      // the loop will be going checking for fault injections until only one operation is demanded
      // the memory is ready for the new process
      else
      {
        ready.write(true);
        ready_value = true;
      }
    }
  }

  void write_process()
  {
    std::cout << "[Write] Starting Write Operation\n";
    // prepare write and read flags for parity for a write operation
    write_en.write(true);
    read_en.write(false);

    // start scrambling the address and encrypting the date(paralell operations)
    start_scramble.write(true);
    start_read_write.write(true);
    // since the operations are paralell ,pick the max latency to wait for the process
    uint32_t latency = std::max(latencyEncrypt, latencyScrambling);
    // wait for the paralell operations to end
    for (size_t i = 0; i < latency; i++)
    {
      wait();
    }
    // scrambling and encrypting done
    start_scramble.write(false);
    start_read_write.write(false); // end scrambling and encryption

    //  in the TA main memory it was checked if the address reaches UINT32_MAX -> overflow
    // can be reached with the prng because the mod is UINT32_MAX +1

    bool overflow = false;
    for (int i = 0; i < 4; i++)
    {
      // store the encrypted data in the scrambled addresses according to endianness
      int index = endianness ? 3 - i : i;
      uint32_t addr = scrambled_addrs[i].read();
      // the check for overflow and the same solution picked by TA :
      // Writes that go outside the memory space can simply be cut off.
      if (addr == UINT32_MAX)
      {
        overflow = true;
        break;
      }

      // handled just like the TA idk???????????????????????
      uint8_t val = encrypted_output[index].read();

      memory[addr] = val;
      std::cout << "[MEMORY][WRITE] Addr: " << addr
                << " = " << (int)val << " (Byte[" << i << "])\n";
      for (size_t i = 0; i < latencyMemoryAccess; i++)
      {
        wait();
      } // wait for the writing in memeory just like in the TA
    }

    // all the work cut off when overflow exists
    if (!overflow)
    {
      for (int i = 0; i < 4; i++)
      {
        // calculate and store parity for the encrypted data
        phys_addr.write(scrambled_addrs[i].read());
        data_in_signal_parity.write(encrypted_output[i].read());
        std::cout << "[PARITY][WRITE] Checking byte " << i << " @ addr "
                  << scrambled_addrs[i].read() << "\n";
        wait();
        start_parity.write(true);
        wait();
        start_parity.write(false);
      }
      // let the user know write was successful
      std::cout << "[Write] Done\n";
    }
    else
    {
      // let the user know the write op was cut off
      std::cout << "[WARNING] Overflow detected (UINT32_MAX). Write aborted.\n";
    }
    // write done:set the flags accordingly
    write_en.write(false);
    error.write(false);
    ready.write(true);
    ready_value = true;
  }

  void read_process()
  {
    std::cout << "[Read] Starting Read Operation\n";
    // prepare write and read flags for parity for a read operation
    read_en.write(true);
    write_en.write(false);

    start_scramble.write(true); // scramble to find the address
    // wait for scrambling first
    for (size_t i = 0; i < latencyScrambling; i++)
    {
      wait();
    }
    start_scramble.write(false); // scrambling over

    // after research : unlike write_op No overflow risk: Reads use pre-scrambled
    // addresses directly (no arithmetic), and std::map safely handles UINT32_MAX as a
    // valid key, making wrap-around impossible.
    for (int i = 0; i < 4; i++)
    {
      // read from memory according to endianness
      int index = endianness ? 3 - i : i;
      uint32_t addr = scrambled_addrs[index].read();
      uint8_t val = getByteAt(addr);
      to_decrypt[i].write(val);
      // wait for accessing the memory to read
      for (size_t i = 0; i < latencyMemoryAccess; i++)
      {
        wait();
      }
      std::cout << "[MEMORY][READ] Addr: " << addr
                << " → " << (int)val << " (Byte[" << i << "])\n";
    }

    start_read_write.write(true); // start decrypting
    // wait for decryption to end
    for (size_t i = 0; i < latencyEncrypt; i++)
    {
      wait();
    }
    start_read_write.write(false); // decrypting over

    sc_uint<32> result = 0; // helper to write in rdata later
    bool failed = false;    // halper tow write in error later+break the work in case of one

    for (int i = 0; i < 4; i++)
    {
      // extract byte for decrypted data for the result
      sc_uint<8> byte = decrypted_output[i].read();
      // extract byte before decryption to compare its parity with the parity of the stored encrypted data
      sc_uint<8> byte_parity = to_decrypt[i].read();
      // add byte to the result
      result |= (sc_uint<32>(byte) << (8 * i));

      phys_addr.write(scrambled_addrs[i].read()); // give the addr to parity to do the checking
      data_in_signal_parity.write(byte_parity);   // give the byte to the parity module
      std::cout << "[PARITY][READ] Byte[" << i << "] = "
                << (int)byte << " @ " << scrambled_addrs[i].read() << "\n";
      start_parity.write(true); // start parity_checking
      wait();
      start_parity.write(false);
      wait(); // end parity_checking
      // in case of a parity_error-> break the loop and set error to be 1 /rdata to be 0
      if (parity_error.read())
      {
        std::cout << "[ERROR] Parity error at byte " << i << "\n";
        error.write(true);
        rdata.write(0);
        failed = true;
        break;
      }
    }
    // in case of no parity_error: write result in rdata and set error flag to be 1
    if (!failed)
    {
      rdata.write(result);
      error.write(false);
      std::cout << "[READ RESULT] rdata = " << result.to_uint() << "\n";
    }
    // read done:set the flags accordingly
    read_en.write(false);
    ready.write(true);
    ready_value = true;
    std::cout << "[Read] Done\n";
  }

  void fault_injection()
  {
    uint32_t addr = fault.read();
    uint8_t bit = faultBit.read().to_uint();

    std::cout << "[FAULT] Injecting Fault @ " << addr
              << " bit " << (int)bit << "\n";
    // when bit between 0 and 7 invert the nten bit in address
    if (bit <= 7)
    {
      sc_uint<8> val = getByteAt(addr);
      memory[addr] = val ^ (1 << bit);
      std::cout << "[FAULT] Data bit flipped: New val @ " << addr
                << " = " << (int)memory[addr] << "\n";
    }
    // if it's 8 :Flip parity bit method exists in parity
    else if (bit == 8)
    {
      parity.inject_parity_fault(addr);
      std::cout << "[FAULT] Parity bit flipped @ " << addr << "\n";
    }
  }

  // Gibt das Byte an der gegebenen physischen Adresse zurüuck.
  sc_uint<8> getByteAt(uint32_t addr)
  {
    if (memory.find(addr) != memory.end())
      return memory[addr];
    // if not found return 0(inspired also by Main Memory)
    return 0;
  }

  // Setters:

  // Setzt den Schlüssel, der zum Address Scrambling verwendet wird:
  // existing method in secure since the scrambling key is calculated there
  void setScramblingKey(uint32_t key)
  {
    std::cout << "[SETUP] Scrambling Key Set: " << key << "\n";
    secure.set_scramble_key(key);
  }

  // Setzt den Schlüssel, der zum Verschlüsseln/Entschlüsseln verwendet wird.
  // existing method in secure since the enc/decryption keys are  calculated there
  void setEncryptionKey(uint32_t addr, uint8_t key)
  {
    std::cout << "[SETUP] Encryption Key Set: Addr = " << addr << ", Key = " << (int)key << "\n";
    secure.set_data_key(addr, key);
  }
  // sets the seed in the secure prng
  void setSeed(uint32_t s)
  {
    seed = s;
    std::cout << "[SETUP] Seed Set: " << s << "\n";
    secure.init_seed(s);
  }

  // sets the endianness
  void setEndianness(uint8_t e)
  {
    endianness = e;
    std::cout << "[SETUP] Endianness Set: " << (int)endianness << "\n";
  }
};

#endif // SECURE_MEMORY_UNIT_HPP