#ifndef PARITY_HPP
#define PARITY_HPP

#include <systemc>
#include <systemc.h>
#include <map>
#include <iostream>
#include <unordered_map>

SC_MODULE(PARITY)
{
    // Ports
    sc_in<bool> start;
    sc_in<sc_uint<32>> physical_addr; // Physical byte address
    sc_in<sc_uint<8>> data_in;        // Data byte to check/write
    sc_in<bool> write_enable;         // High during write operations
    sc_in<bool> read_enable;          // High during read operations

    sc_out<bool> parity_error; // High when parity check fails

    // Internal State
    std::unordered_map<uint32_t, uint8_t> parity_map; // Packed parity map (8 bits per entry)

    SC_CTOR(PARITY)
    {
        SC_METHOD(process);
        sensitive << start.pos();
    }

    // Calculate parity (even parity)
    bool calculate_parity(sc_uint<8> data)
    {
        bool parity = 0;
        for (int i = 0; i < 8; i++)
        {
            parity ^= data[i];
        }
        return parity;
    }

    void set_bit(uint32_t address)
    {
        uint32_t byte_index = address / 8;
        uint8_t bit_index = address % 8;
        parity_map[byte_index] |= (1 << bit_index);
    }

    void clear_bit(uint32_t address)
    {
        uint32_t byte_index = address / 8;
        uint8_t bit_index = address % 8;
        parity_map[byte_index] &= ~(1 << bit_index);
    }

    bool check_bit(uint32_t address) const
    {
        uint32_t byte_index = address / 8;
        uint8_t bit_index = address % 8;
        auto it = parity_map.find(byte_index);
        if (it != parity_map.end())
        {
            return (it->second >> bit_index) & 1;
        }
        return false; // Default if not found
    }

    // Main Process
    void process()
    {
        uint32_t addr = physical_addr.read();
        sc_uint<8> data = data_in.read();
        bool parity = calculate_parity(data);

        if (write_enable.read())
        {
            if (parity)
                set_bit(addr);
            else
                clear_bit(addr);

            parity_error.write(false); // always false during write
        }
        else if (read_enable.read())
        {
            bool stored_parity = check_bit(addr);
            if (parity != stored_parity)
            {
                parity_error.write(true);
            }
            else
            {
                parity_error.write(false);
            }
        }
    }

    // Fault injection: flips the parity bit
    void inject_parity_fault(uint32_t address)
    {
        uint32_t byte_index = address / 8;
        uint8_t bit_index = address % 8;

        parity_map[byte_index] ^= (1 << bit_index); // Flip bit
    }

    // Returns the parity bit for the given address.
    bool get_parity(uint32_t address) const
    {
        return check_bit(address);
    }
};
#endif // PARITY_HPP
