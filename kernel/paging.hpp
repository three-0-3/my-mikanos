#pragma once

#include <cstddef>

// create page table of identity mapping (virtual address = physical address)
// set CR3 register
void SetupIdentityPageTable();

void InitializePaging();