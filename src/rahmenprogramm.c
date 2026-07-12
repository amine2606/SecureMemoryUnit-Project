#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "../include/relevant_structs.h"
#include "../include/run_simulation.hpp"
static int requests_counter;
extern struct Result run_simulation(
    uint32_t cycles,
    const char *tracefile,
    uint8_t endianness,
    uint32_t latencyScrambling,
    uint32_t latencyEncrypt,
    uint32_t latencyMemoryAccess,
    uint32_t seed,
    uint32_t numRequests,
    struct Request *requests);

void print_usage(const char *prog_name)
{
  printf("Usage: %s [OPTIONS] <input_file>\n", prog_name);
  printf("Options:\n");
  printf("  --cycles, -c <number>             Number of simulation cycles (default: 1000)\n");
  printf("  --tf, -t <tracefile>              Path to the trace file (optional)\n");
  printf("  --endianness, -e <number>         Endianness (default: 0)\n");
  printf("  --latency-scrambling, -i <number> Latency for address scrambling (default: 1)\n");
  printf("  --latency-encrypt, -j <number>    Latency for encryption/decryption (default: 3)\n");
  printf("  --latency-memory-access, -k <number> Latency for memory access (default: 4)\n");
  printf("  --seed, -s <number>               Seed for the pRNG (default: 1234)\n");
  printf("  --help, -h                       Display this help message\n");
  printf("Arguments:\n");
  printf("  <input_file>                     Path to the input CSV file with requests\n");
}
/**
 * Parses a CSV file containing SMU requests into an array of Request structs.
 *
 * @param filename Path to the CSV file to parse
 * @param requests_ptr Output pointer to store allocated array of requests
 * @param numRequests_ptr Output pointer to store number of parsed requests
 * @return 0 on success, -1 on error
 */
int parse_requests(const char *filename, struct Request **requests_ptr, uint32_t *numRequests_ptr)
{
  // Open file and check for errors
  FILE *file = fopen(filename, "r");
  if (!file)
  {
    perror("Error opening file");
    return -1;
  }

  // First pass: count the number of valid requests
  char line[256];
  uint32_t count = 0;

  while (fgets(line, sizeof(line), file))
  {
    // Check for line length overflow (incomplete line read)
    if (line[strlen(line) - 1] != '\n' && !feof(file))
    {
      fprintf(stderr, "Error: Line exceeds maximum allowed length of %d characters.\n", 255);
      fclose(file);
      return -1;
    }

    // Skip empty lines or comment lines (starting with #)
    if (strspn(line, " \t\r\n") == strlen(line) || line[0] == '#')
    {
      continue;
    }

    count++;
  }

  // Safety check against integer overflow
  if (count >= UINT32_MAX)
  {
    fprintf(stderr, "Exceeded maximum number of requests (%d)\n", 256);
    fclose(file);
    return -1;
  }

  // Allocate memory for the exact number of requests we found
  struct Request *requests = (struct Request *)malloc(count * sizeof(struct Request));
  if (requests == NULL)
  {
    fprintf(stderr, "Error: Memory allocation failed for requests.\n");
    fclose(file);
    return -1;
  }

  // Second pass: actual parsing of each line into Request structs
  rewind(file);
  uint32_t countReq = 0;

  while (fgets(line, sizeof(line), file))
  {
    // Remove newline character
    line[strcspn(line, "\n")] = 0;

    // Skip empty lines or comments again
    if (strlen(line) == 0 || line[0] == '#')
    {
      continue;
    }
    // Initialize request with default values
    struct Request req = {0, 0, 0, 0, UINT32_MAX, 0};
    char type;

    // Tokenize the CSV line into 5 possible fields
    char *tokens[5] = {NULL};
    int token_count = 0;

    // Custom tokenizer that handles empty fields
    char *start = line;
    while (start != NULL && token_count < 5)
    {
      char *end = strchr(start, ','); // Find the next comma

      if (end != NULL)
      {
        *end = '\0'; // Temporarily terminate  at the comma
      }

      // Store token if non-empty, otherwise mark as empty
      if (start != NULL && *start != '\0')
      {
        tokens[token_count++] = start;
      }
      else
      {
        tokens[token_count++] = "empty";
      }

      // Move to next field
      start = (end != NULL) ? end + 1 : NULL;
    }

    // Ensure we have exactly 5 tokens (pad with "empty" if needed)
    while (token_count < 5)
    {
      tokens[token_count++] = "empty";
    }

    // Debug print of parsed tokens
    printf("Request read: Type = %s, Addr = %s, Data = %s, Fault = %s, Fault-Bit = %s\n",
           tokens[0], tokens[1], tokens[2], tokens[3], tokens[4]);
    // Validate request type format (single character)
    if (strlen(tokens[0]) > 1 && countReq > 0)
    {
      fprintf(stderr, "Error: Invalid Type Format %s\n", line);
      fclose(file);
      free(requests);
      return -1;
    }
    type = tokens[0][0];
    // Process based on request type
    if (type == 'R' || type == 'W')
    {
      // Parse address field (required for R/W)
      if (strcmp(tokens[1], "empty") != 0)
      {
        int base = 10; // Default  decimal
        if (tokens[1][0] == '0' && (tokens[1][1] == 'x' || tokens[1][1] == 'X'))
        {
          base = 16; // Hexadecimal
        }
        req.addr = (uint32_t)strtoul(tokens[1], NULL, base);
      }
      else
      {
        fprintf(stderr, "Error: Malformed request: no address %s\n", line);
        fclose(file);
        free(requests);
        return -1;
      }

      // Parse optional fault injection parameters
      if (token_count == 5 && strcmp(tokens[3], "empty") != 0 && strcmp(tokens[4], "empty") != 0)
      {
        int base = 10; // default decimal
        if (tokens[3][0] == '0' && (tokens[3][1] == 'x' || tokens[3][1] == 'X'))
        {
          base = 16; // Hexadecimal
        }
        req.fault = (uint32_t)strtoul(tokens[3], NULL, base);
        // Fault bit must be decimal
        if (tokens[4][0] == '0' && (tokens[4][1] == 'x' || tokens[4][1] == 'X'))
        {
          fprintf(stderr, "Error: fault bit is given in Hex in the last treated request\n");
          fclose(file);
          free(requests);
          return -1;
        }
        req.faultBit = (uint8_t)strtoul(tokens[4], NULL, 10);
      }
      else if ((strcmp(tokens[3], "empty") == 0 && strcmp(tokens[4], "empty") != 0) ||
               (strcmp(tokens[3], "empty") != 0 && strcmp(tokens[4], "empty") == 0))
      {
        fprintf(stderr, "Error: Malformed fault request: fault injection incomplete in %s\n", line);
        fclose(file);
        free(requests);
        return -1;
      }
      // Handle Read-specific validation
      if (type == 'R')
      {
        if (strcmp(tokens[2], "empty") != 0)
        {
          fprintf(stderr, "Error: Malformed Read request: Data in R not empty %s\n", line);
          fclose(file);
          free(requests);
          return -1;
        }
        req.r = 1;
        req.w = 0;
      }
      else
      { // type W
        // data must be non-empty for write requests
        if (strcmp(tokens[2], "empty") != 0)
        {
          int base = 10; // default decimal
          if (tokens[2][0] == '0' && (tokens[2][1] == 'x' || tokens[2][1] == 'X'))
          {
            base = 16; // Hexadecimal
          }
          req.data = (uint32_t)strtoul(tokens[2], NULL, base);
        }
        else
        {
          fprintf(stderr, "Error: Malformed Write request: Data in W empty %s\n", line);
          fclose(file);
          free(requests);
          return -1;
        }
        req.w = 1;
        req.r = 0;
      }
    }
    else if (type == 'F')
    {
      // For fault requests, fields 1 and 2 should be empty, 3 and 4 should be present
      if ((strcmp(tokens[1], "empty") != 0 || strcmp(tokens[2], "empty") != 0) ||
          (strcmp(tokens[3], "empty") == 0 || strcmp(tokens[4], "empty") == 0))
      {
        fprintf(stderr, "Error: Malformed fault request: data and address non empty or fault fields empty %s\n", line);
        fclose(file);
        free(requests);
        return -1;
      }
      else
      {
        int base = 10; // default decimal
        if (tokens[3][0] == '0' && (tokens[3][1] == 'x' || tokens[3][1] == 'X'))
        {
          base = 16; // Hexadecimal
        }
        req.fault = (uint32_t)strtoul(tokens[3], NULL, base);

        if (tokens[4][0] == '0' && (tokens[4][1] == 'x' || tokens[4][1] == 'X'))
        {
          fprintf(stderr, "Error: fault bit is given in Hex in the last treated request\n");
          fclose(file);
          free(requests);
          return -1;
        }
        req.faultBit = (uint8_t)strtoul(tokens[4], NULL, 10);
      }
    }
    else if (strcmp(tokens[0], "Type") == 0 &&
             strcmp(tokens[1], "Address") == 0 &&
             strcmp(tokens[2], "Data") == 0 &&
             strcmp(tokens[3], "Fault") == 0 && strcmp(tokens[4], "Fault-Bit") == 0 && countReq == 0)
    {
      continue;
    }
    else
    {
      fprintf(stderr, "Error: Invalid request type in line: %s\n", line);
      fclose(file);
      free(requests);
      return -1;
    }

    requests[countReq++] = req;
  }

  fclose(file);

  // Set the output parameters
  *requests_ptr = requests;
  *numRequests_ptr = countReq;

  return 0;
}

int main(int argc, char *argv[])
{
  // default values
  uint32_t cycles = 1000;
  const char *tracefile = NULL;
  const char *input_file = NULL;
  uint8_t endianness = 0;
  uint32_t latencyScrambling = 1;
  uint32_t latencyEncrypt = 3;
  uint32_t latencyMemoryAccess = 4;
  uint32_t seed = 1234;

  static struct option long_options[] = {
      {"cycles", required_argument, 0, 'c'},
      {"tf", required_argument, 0, 't'},
      {"endianness", required_argument, 0, 'e'},
      {"latency-scrambling", required_argument, 0, 'i'},
      {"latency-encrypt", required_argument, 0, 'j'},
      {"latency-memory-access", required_argument, 0, 'k'},
      {"seed", required_argument, 0, 's'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "c:t:e:i:j:k:s:h", long_options, &option_index)) != -1)
  {
    switch (opt)
    {
    case 'c':
      cycles = strtoul(optarg, NULL, 10);
      if (cycles == 0 || cycles >= UINT32_MAX)
      {
        fprintf(stderr, "Invalid number of cycles.\n");
        return 1;
      }
      break;
    case 't':
      tracefile = optarg;
      break;
    case 'e':
      endianness = strtoul(optarg, NULL, 10);
      if (endianness > 1)
      {
        fprintf(stderr, "Invalid endianness value : Please use 0 or 1 .\n");
        return 1;
      }
      break;
    case 'i':
      latencyScrambling = strtoul(optarg, NULL, 10);
      if (latencyScrambling == 0 || latencyScrambling > UINT32_MAX)
      {
        fprintf(stderr, "Invalid value for latencyScrambling \n");
        return 1;
      }
      break;
    case 'j':
      latencyEncrypt = strtoul(optarg, NULL, 10);
      if (latencyEncrypt < 2 || latencyEncrypt > UINT32_MAX)
      {
        fprintf(stderr, "Invalid value for latencyEncrypt \n");
        return 1;
      }
      break;
    case 'k':
      latencyMemoryAccess = strtoul(optarg, NULL, 10);
      if (latencyMemoryAccess < 4 || latencyMemoryAccess > UINT32_MAX)
      {
        fprintf(stderr, "Invalid value for latencyMemoryAccess \n");
        return 1;
      }
      break;
    case 's':
      seed = strtoul(optarg, NULL, 10);
      if (seed > UINT32_MAX)
      {
        fprintf(stderr, "Invalid value for seed\n");
        return 1;
      }
      break;

    case 'h':
      print_usage(argv[0]);
      return 0;

    default:
      break;
    }
  }

  if (optind < argc)
  {
    input_file = argv[optind];
  }
  else
  {
    fprintf(stderr, "Input file is required.\n");
    print_usage(argv[0]);
    return 1;
  }
  optind++;
  if (optind < argc)
  {
    fprintf(stderr, "Unexpected argument(s):");
    for (int i = optind; i < argc; i++)
    {
      fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr, "\n");
    print_usage(argv[0]);
    return 1;
  }
  struct Request *requests = NULL;
  uint32_t numRequests = 0;
  if (parse_requests(input_file, &requests, &numRequests) != 0)
  {
    free(requests);
    return 1;
  }
  struct Result result = run_simulation(
      cycles,
      tracefile,
      endianness,
      latencyScrambling,
      latencyEncrypt,
      latencyMemoryAccess,
      seed,
      numRequests,
      requests);

  free(requests);
  printf("\nSimulation Results:\n");
  printf("  Total Cycles: %u\n", result.cycles);
  printf("  Errors Detected: %u\n", result.errors);

  return 0;
}