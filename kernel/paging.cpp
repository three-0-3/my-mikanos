#include "paging.hpp"

#include <array>
#include <cstdint>

#include "asmfunc.h"
#include "memory_manager.hpp"
#include "task.hpp"

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

namespace {

WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
  if (entry.bits.present) {
    return { entry.Pointer(), MAKE_ERROR(Error::kSuccess) };
  }

  auto [ child_map, err ] = NewPageMap();
  if (err) {
    return { nullptr, err };
  }

  entry.SetPointer(child_map);
  entry.bits.present = 1;

  return { child_map, MAKE_ERROR(Error::kSuccess) };
}

WithError<size_t> SetupPageMap(PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  while (num_4kpages > 0) {
    const auto entry_index = addr.Part(page_map_level);

    auto [ child_map, err ] = SetNewPageMapIfNotPresent(page_map[entry_index]);
    if (err) {
      return { num_4kpages, err };
    }
    page_map[entry_index].bits.writable = 1;
    page_map[entry_index].bits.user = 1;

    if (page_map_level == 1) {
      --num_4kpages;
    } else {
      auto [ num_remain_pages, err ] = SetupPageMap(child_map, page_map_level - 1, addr, num_4kpages);
      if (err) {
        return { num_4kpages, err };
      }
      num_4kpages = num_remain_pages;
    }

    if (entry_index == 511) {
      break;
    }

    addr.SetPart(page_map_level, entry_index + 1);
    for(int level = page_map_level - 1; level >= 1; --level) {
      addr.SetPart(level, 0);
    }
  }

  return { num_4kpages, MAKE_ERROR(Error::kSuccess) };
}

Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
  for (int i = 0; i < 512; ++i) {
    auto entry = page_map[i];
    if (!entry.bits.present) {
      continue;
    }

    if (page_map_level > 1) {
      if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if (auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }
    page_map[i].data = 0;
  }

  return MAKE_ERROR(Error::kSuccess);
}

} // namespace

WithError<PageMapEntry*> NewPageMap() {
  auto frame = memory_manager->Allocate(1);
  if (frame.error) {
    return { nullptr, frame.error };
  }

  auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return { e, MAKE_ERROR(Error::kSuccess)};
}

Error FreePageMap(PageMapEntry* table) {
  const FrameID frame{reinterpret_cast<uintptr_t>(table) / kBytesPerFrame};
  return memory_manager->Free(frame, 1);
}

Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if (auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}

Error HandlePageFault(uint64_t error_code, uint64_t causal_addr) {
  auto& task = task_manager->CurrentTask();
  if (error_code & 1) { // P=1 interrupt by permission violation at page level
    return MAKE_ERROR(Error::kAlreadyAllocated);
  }
  if (causal_addr < task.DPagingBegin() || task.DPagingEnd() <= causal_addr) {
    return MAKE_ERROR(Error::kIndexOutOfRange);
  }
  return SetupPageMaps(LinearAddress4Level{causal_addr}, 1);
}