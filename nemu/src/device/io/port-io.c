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

#define PORT_IO_SPACE_MAX 65535

#define NR_MAP 16
static IOMap maps[NR_MAP] = {};
static int nr_map = 0;

/* device interface */
void add_pio_map(const char *name, ioaddr_t addr, void *space, uint32_t len, io_callback_t callback) {
  assert(nr_map < NR_MAP);
  assert(addr + len <= PORT_IO_SPACE_MAX);
  maps[nr_map] = (IOMap){ .name = name, .low = addr, .high = addr + len - 1,
    .space = space, .callback = callback };
  Log("Add port-io map '%s' at [" FMT_PADDR ", " FMT_PADDR "]",
      maps[nr_map].name, maps[nr_map].low, maps[nr_map].high);

  nr_map ++;
}

/* CPU interface */
uint32_t pio_read(ioaddr_t addr, int len) {
  assert(addr + len - 1 < PORT_IO_SPACE_MAX);
  int mapid = find_mapid_by_addr(maps, nr_map, addr);
  assert(mapid != -1);
  return map_read(addr, len, &maps[mapid]);
}

/**
 * Writes data to a specified I/O address space.
 *
 * This function writes a given 32-bit data value to a specified I/O address space.
 * The address space is defined by the `addr` parameter, and the length of the data
 * to be written is specified by the `len` parameter. The function ensures that the
 * address range (from `addr` to `addr + len - 1`) is within the valid I/O port space
 * by checking against `PORT_IO_SPACE_MAX`. If the address range is invalid, the
 * function will trigger an assertion failure.
 *
 * The function also identifies the appropriate memory map associated with the given
 * address by calling `find_mapid_by_addr`. If no valid map is found (i.e., `mapid` is -1),
 * an assertion failure is triggered. Finally, the function writes the data to the
 * identified memory map using `map_write`.
 *
 * @param addr The starting I/O address where the data will be written.
 * @param len The length of the data to be written (in bytes).
 * @param data The 32-bit data value to be written to the I/O address space.
 *
 * @note This function uses assertions to validate the address range and the memory map,
 *       which means it is intended for debugging purposes. In production code, proper
 *       error handling should replace these assertions.
 */
void pio_write(ioaddr_t addr, int len, uint32_t data) {
  assert(addr + len - 1 < PORT_IO_SPACE_MAX);
  int mapid = find_mapid_by_addr(maps, nr_map, addr);
  assert(mapid != -1);
  map_write(addr, len, data, &maps[mapid]);
}
