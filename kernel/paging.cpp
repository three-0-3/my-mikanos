#include "paging.hpp"

#include <array>
#include <cstdint>

#include "asmfunc.h"

namespace {
  const uint64_t kPageSize4K = 4096;
  const uint64_t kPageSize2M = 512 * kPageSize4K;
  const uint64_t kPageSize1G = 512 * kPageSize2M;

  // the number of the page directory to be allocated staticly
  // a page directory contains 512 pages, a page is 2MB
  // hence, kPageDirectoryCount * 512 * 2MB = 64GB virtual addresses are mapped
  const size_t kPageDirectoryCount = 64;

  alignas(kPageSize4K) std::array<uint64_t, 512> pml4_table;
  alignas(kPageSize4K) std::array<uint64_t, 512> pdp_table; // not an array since we prepare only one pdp here
  alignas(kPageSize4K) std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;
}

void SetupIdentityPageTable() {
  pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x03;
  for (int i_pdpt = 0; i_pdpt < page_directory.size(); ++i_pdpt) {
    pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&page_directory[i_pdpt]) | 0x03;
    for (int i_pd = 0; i_pd < 512; ++i_pd) {
      page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x83;
    }
  }

  ResetCR3();
}

void InitializePaging() {
  SetupIdentityPageTable();
}

void ResetCR3() {
  SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}