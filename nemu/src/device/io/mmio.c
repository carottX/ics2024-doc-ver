/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <device/map.h>
#include <memory/paddr.h>

#define NR_MAP 16

static IOMap maps[NR_MAP] = {};
static int nr_map = 0;

/**
 * Fetches the memory-mapped I/O (MMIO) map corresponding to the given physical address.
 *
 * This function searches for the MMIO map that contains the specified physical address `addr`.
 * It uses the `find_mapid_by_addr` function to determine the index of the map in the `maps` array.
 * If a valid map ID is found, a pointer to the corresponding MMIO map is returned. If no map
 * contains the given address (i.e., `find_mapid_by_addr` returns -1), the function returns NULL.
 *
 * @param addr The physical address to search for within the MMIO maps.
 * @return A pointer to the MMIO map containing the address, or NULL if no such map exists.
 */
static IOMap* fetch_mmio_map(paddr_t addr) {
  int mapid = find_mapid_by_addr(maps, nr_map, addr);
  return (mapid == -1 ? NULL : &maps[mapid]);
}

/**
 * Reports an overlap between two MMIO (Memory-Mapped I/O) regions and triggers a panic.
 *
 * This function is used to detect and report when two MMIO regions overlap in memory.
 * It takes the names and address ranges of the two regions as input and constructs a
 * panic message indicating the overlap. The panic message includes the names of the
 * regions and their respective address ranges.
 *
 * @param name1 The name of the first MMIO region.
 * @param l1    The starting address of the first MMIO region.
 * @param r1    The ending address of the first MMIO region.
 * @param name2 The name of the second MMIO region.
 * @param l2    The starting address of the second MMIO region.
 * @param r2    The ending address of the second MMIO region.
 *
 * @note This function will cause the system to panic immediately upon invocation,
 *       halting further execution.
 */
static void report_mmio_overlap(const char *name1, paddr_t l1, paddr_t r1,
    const char *name2, paddr_t l2, paddr_t r2) {
  panic("MMIO region %s@[" FMT_PADDR ", " FMT_PADDR "] is overlapped "
               "with %s@[" FMT_PADDR ", " FMT_PADDR "]", name1, l1, r1, name2, l2, r2);
}

/* device interface */
void add_mmio_map(const char *name, paddr_t addr, void *space, uint32_t len, io_callback_t callback) {
  assert(nr_map < NR_MAP);
  paddr_t left = addr, right = addr + len - 1;
  if (in_pmem(left) || in_pmem(right)) {
    report_mmio_overlap(name, left, right, "pmem", PMEM_LEFT, PMEM_RIGHT);
  }
  for (int i = 0; i < nr_map; i++) {
    if (left <= maps[i].high && right >= maps[i].low) {
      report_mmio_overlap(name, left, right, maps[i].name, maps[i].low, maps[i].high);
    }
  }

  maps[nr_map] = (IOMap){ .name = name, .low = addr, .high = addr + len - 1,
    .space = space, .callback = callback };
  Log("Add mmio map '%s' at [" FMT_PADDR ", " FMT_PADDR "]",
      maps[nr_map].name, maps[nr_map].low, maps[nr_map].high);

  nr_map ++;
}

/* bus interface */
word_t mmio_read(paddr_t addr, int len) {
  return map_read(addr, len, fetch_mmio_map(addr));
}

/**
 * Writes data to a memory-mapped I/O (MMIO) address.
 *
 * This function writes a specified amount of data to a memory-mapped I/O address.
 * It first fetches the appropriate MMIO mapping for the given address using
 * `fetch_mmio_map`, then performs the write operation using `map_write`.
 *
 * @param addr The physical address to write to.
 * @param len  The length of the data to write, in bytes.
 * @param data The data to write to the specified address.
 */
void mmio_write(paddr_t addr, int len, word_t data) {
  map_write(addr, len, data, fetch_mmio_map(addr));
}
