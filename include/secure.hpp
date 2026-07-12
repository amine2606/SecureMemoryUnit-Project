#ifndef SECURE_HPP
#define SECURE_HPP

#include <systemc>
#include <systemc.h>
#include <map>
#include <array>
#include <iostream>
#include <unordered_map>

SC_MODULE(SECURE)
{
  // Input ports
  sc_in<bool> start_scramble;                               // triggers the scrambling process when enabled
  sc_in<bool> start_read_write;                             // triggers the encryption (write operation) or the decryption (read operation)
  sc_in<bool> w_oder_r;                                     // a flag to determine which process is taking place (Write = 1, Read = 0) and enc-/decrypt accordingly
  sc_in<sc_uint<32>> addr;                                  // connects to the input port addr from SecureMemoryUnit
  sc_in<sc_uint<32>> wdata;                                 // connects to the input port wdata from SecureMemoryUnit
  sc_vector<sc_in<sc_uint<8>>> to_decrypt{"to_decrypt", 4}; // 4 input ports for 8-bit values to be decrypted (one per byte)

  // Output ports
  sc_vector<sc_out<sc_uint<32>>> scrambled_addrs{"scrambled_addrs", 4};  // 4 output ports for 32-bit scrambled physical addresses (one per byte)
  sc_vector<sc_out<sc_uint<8>>> encrypted_output{"encrypted_output", 4}; // 4 output ports for 8-bit encrypted byte values
  sc_vector<sc_out<sc_uint<8>>> decrypted_output{"decrypted_output", 4}; // 4 output ports for 8-bit decrypted byte values

  // Internal State
  std::map<sc_uint<32>, sc_uint<8>> data_keys;      // per-address data keys
  std::map<sc_uint<32>, sc_uint<32>> scramble_keys; // saves the address and the key uses in its scrambling proccess
  sc_uint<32> scramble_key;
  sc_uint<32> prng_state; // stores the last generated key
  const sc_uint<32> LCG_MULT = 1664525;
  const sc_uint<32> LCG_INCR = 1013904223;

  SC_CTOR(SECURE)
  {
    SC_METHOD(secure);
    sensitive << start_read_write.pos();
    sensitive << start_scramble.pos();
  }

  // Main function of the module SECURE:
  // Takes care of all the processes in which we need to generate a key whether it's for the address scrambling or dealing with data (encryption/decryption)
  void secure()
  {
    sc_uint<32> current_addr = addr.read();

    // * Address Scrambling Process *
    if (start_scramble.read())
    {
      std::cout << "\n=== [START SCRAMBLE] ===" << std::endl;
      // check if a scrambling key for the current_addr was already generated and saved in scramble_keys map.
      if (scramble_keys.count(current_addr)) // if yes fetch the key from the map
      {
        scramble_key = scramble_keys[current_addr];
        std::cout << "[SCRAMBLE] Existing key for address " << current_addr.to_uint()
                  << " = " << scramble_key.to_uint() << std::endl;
      }
      else // if not generate a new one and save it to the map
      {
        scramble_key = generate_prng();
        scramble_keys[current_addr] = scramble_key;
        std::cout << "[SCRAMBLE] New key for address " << current_addr.to_uint()
                  << " = " << scramble_key.to_uint() << std::endl;
      }
      // scramble the current_addr as described in [1] (see line 117)
      generate_scrambled_addrs(current_addr);
    }
    // * Data Encryption (during a write operation) or Decryption (During a read operation) *
    if (start_read_write.read())
    {
      std::cout << "\n=== [START READ/WRITE] ===" << std::endl;
      // Write operation --> Data Encryption
      if (w_oder_r.read())
      {
        std::cout << "[WRITE] Writing to address " << current_addr.to_uint() << std::endl;
        // get the key that will be used for the encryption [2] (see line 133)
        sc_uint<8> key = get_data_key(current_addr);
        // encrypt data [3] (see line 153)
        sc_uint<8> encrypted[4];
        process_data(wdata.read(), key, encrypted);
        for (int i = 0; i < 4; i++)
        {
          encrypted_output[i].write(encrypted[i]); // Saving the encrypted bytes -one at a time- to encrypted_output which will connet to the SMU
        }
      }
      // Read operation --> Data Decryption
      else
      {
        std::cout << "[READ] Reading from address " << current_addr.to_uint() << std::endl;
        // Check if keys for current_addr exist (True when Read after Write)
        if (data_keys.count(current_addr))
        {
          sc_uint<8> key = data_keys[current_addr]; // Get the key used in the Encryption
          for (int i = 0; i < 4; i++)
          {
            sc_uint<8> encrypted = to_decrypt[i].read(); // Encrypted byte
            sc_uint<8> decrypted = encrypted ^ key;      // Decryption: Xoring the byte with the same key that encrypted it
            decrypted_output[i].write(decrypted);        // Saving the decrypted bytes -one at a time- to decrypted_output which will connet to the SMU
            std::cout << "  [DECRYPT] Byte[" << i << "] = " << encrypted.to_uint()
                      << " XOR " << key.to_uint() << " = " << decrypted.to_uint() << std::endl;
          }
        }
        else // If not print error message
        {
          std::cout << "[READ] No data key found for address!" << std::endl;
        }
      }
    }
  }

  // Generates the next random 32-bit key
  sc_uint<32> generate_prng()
  {
    // LCG-formula source: https://en.wikipedia.org/wiki/Linear_congruential_generator
    prng_state = (prng_state * LCG_MULT + LCG_INCR) % 4294967296; // LCG_MOD = 2^32 (since a 32-bit key is needed)
    std::cout << sc_time_stamp() << ": [PRNG] New PRNG value = "
              << prng_state.to_uint() << std::endl;
    return prng_state;
  }

  // [1] This function scrambles the 32-bit base address by XORing each of its 4 byte addresses
  // (base_addr to base_addr + 3) with the generated 32-bit scramble key.
  void generate_scrambled_addrs(sc_uint<32> base_addr)
  {
    std::cout << sc_time_stamp() << ": [SCRAMBLE] Using scramble key = "
              << scramble_key.to_uint() << std::endl;
    for (int i = 0; i < 4; i++)
    {
      sc_uint<32> byte_addr = base_addr + i;
      sc_uint<32> scrambled = byte_addr ^ scramble_key;
      scrambled_addrs[i].write(scrambled); // The resulting scrambled addresses are stored in scrambled_addrs.
      std::cout << "  [SCRAMBLE] Byte Addr: " << byte_addr.to_uint()
                << " XOR " << scramble_key.to_uint() << " = " << scrambled.to_uint() << std::endl;
    }
  }

  // [2] This function takes an address, checks if a corresponding key was already generated.
  // If not new 8-bit key will be generated, saved in data_keys for later use then returned (generally during Write Operations).
  // Else the previously saved key will be returned (generally during Read Operations).
  sc_uint<8> get_data_key(sc_uint<32> addr)
  {
    if (!data_keys.count(addr))
    {
      data_keys[addr] = generate_prng().range(7, 0);
      std::cout << "[KEYGEN] New data key for address " << addr.to_uint()
                << " = " << data_keys[addr].to_uint() << std::endl;
    }
    else
    {
      std::cout << "[KEYGEN] Using existing data key for address " << addr.to_uint()
                << " = " << data_keys[addr].to_uint() << std::endl;
    }
    return data_keys[addr];
  }

  // [3] The function takes the data to be encrypted (1st argument), XORes each byte of it using the encryption key (2nd argument)
  // and stores each encrypted byte in the result array (3rd argument)
  void process_data(sc_uint<32> data, sc_uint<8> key, sc_uint<8> result[4])
  {
    std::cout << sc_time_stamp() << ": [ENCRYPT] Encrypting " << data.to_uint()
              << " with key " << key.to_uint() << std::endl;

    for (int i = 0; i < 4; i++)
    {
      sc_uint<8> byte = data.range(7 + 8 * i, 8 * i);
      result[i] = byte ^ key;
      std::cout << "  [ENCRYPT] Byte[" << i << "] = " << byte.to_uint()
                << " XOR " << key.to_uint() << " = " << result[i].to_uint() << std::endl;
    }
  }

  void set_scramble_key(sc_uint<32> key) { scramble_key = key; }
  void set_data_key(sc_uint<32> addr, sc_uint<8> key) { data_keys[addr] = key; }
  void init_seed(uint32_t seed) { prng_state = seed; }
};

#endif // SECURE_HPP