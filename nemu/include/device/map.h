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

#ifndef __DEVICE_MAP_H__
#define __DEVICE_MAP_H__

#include <cpu/difftest.h>

typedef void(*io_callback_t)(uint32_t, int, bool);
uint8_t* new_space(int size);

typedef struct {
  const char *name;
  // we treat ioaddr_t as paddr_t here
  paddr_t low;
  paddr_t high;
  void *space;
  io_callback_t callback;
} IOMap;

/**
 * @brief Checks if a given physical address is within the bounds of an I/O map.
 *
 * This function determines whether the specified physical address `addr` lies
 * within the range defined by the `low` and `high` fields of the provided `IOMap`
 * structure. The address is considered to be inside the map if it is greater than
 * or equal to `map->low` and less than or equal to `map->high`.
 *
 * @param map Pointer to the IOMap structure containing the address range.
 * @param addr The physical address to check.
 * @return `true` if the address is within the map's bounds, `false` otherwise.
 */
static inline bool map_inside(IOMap *map, paddr_t addr) {
  return (addr >= map->low && addr <= map->high);
}

/**
 * Searches for the index of the IOMap structure within the given array that contains the specified physical address.
 *
 * This function iterates through the provided array of IOMap structures and checks if the given physical address
 * falls within the range defined by any of the IOMap structures. If a matching IOMap is found, the function
 * calls `difftest_skip_ref()` and returns the index of the matching IOMap. If no matching IOMap is found,
 * the function returns -1.
 *
 * @param maps Pointer to the array of IOMap structures to search through.
 * @param size The number of IOMap structures in the array.
 * @param addr The physical address to search for within the IOMap structures.
 * @return The index of the IOMap structure that contains the address, or -1 if no such IOMap is found.
 */
static inline int find_mapid_by_addr(IOMap *maps, int size, paddr_t addr) {
  int i;
  for (i = 0; i < size; i ++) {
    if (map_inside(maps + i, addr)) {
      difftest_skip_ref();
      return i;
    }
  }
  return -1;
}

void add_pio_map(const char *name, ioaddr_t addr,
        void *space, uint32_t len, io_callback_t callback);
void add_mmio_map(const char *name, paddr_t addr,
        void *space, uint32_t len, io_callback_t callback);

word_t map_read(paddr_t addr, int len, IOMap *map);
void map_write(paddr_t addr, int len, word_t data, IOMap *map);

#endif
