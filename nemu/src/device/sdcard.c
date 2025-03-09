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
#include "mmc.h"

// http://www.files.e-shop.co.il/pdastore/Tech-mmc-samsung/SEC%20MMC%20SPEC%20ver09.pdf

// see page 26 of the manual above
#define MEMORY_SIZE (16ull * 1024 * 1024 * 1024)  // 16GB
#define READ_BL_LEN 15
#define BLOCK_LEN (1 << READ_BL_LEN)
#define NR_BLOCK (MEMORY_SIZE / BLOCK_LEN)
#define C_SIZE_MULT 7  // only 3 bits
#define MULT (1 << (C_SIZE_MULT + 2))
#define C_SIZE (NR_BLOCK / MULT - 1)

// This is a simple hardware implementation of linux/drivers/mmc/host/bcm2835.c
// No DMA and IRQ is supported, so the driver must be modified to start PIO
// right after sending the actual read/write commands.

enum {
  SDCMD, SDARG, SDTOUT, SDCDIV,
  SDRSP0, SDRSP1, SDRSP2, SDRSP3,
  SDHSTS, __PAD0, __PAD1, __PAD2,
  SDVDD, SDEDM, SDHCFG, SDHBCT,
  SDDATA, __PAD10, __PAD11, __PAD12,
  SDHBLC
};

static FILE *fp = NULL;
static uint32_t *base = NULL;
static uint32_t blkcnt = 0;
static long blk_addr = 0;
static uint32_t addr = 0;
static bool write_cmd = 0;
static bool read_ext_csd = false;

/**
 * Prepares the system for a read or write operation based on the provided flag.
 * 
 * This function sets the block address (`blk_addr`) to the value from the base array at the index specified by `SDARG`.
 * It initializes the `addr` variable to 0, indicating the starting position for the operation. If a file pointer (`fp`)
 * is available, it seeks to the position in the file corresponding to the block address shifted left by 9 bits (equivalent
 * to multiplying by 512, assuming block size is 512 bytes). Finally, it sets the `write_cmd` flag to indicate whether
 * the operation is a write (non-zero) or a read (zero).
 *
 * @param is_write An integer flag indicating the type of operation: non-zero for write, zero for read.
 */
static void prepare_rw(int is_write) {
  blk_addr = base[SDARG];
  addr = 0;
  if (fp) fseek(fp, blk_addr << 9, SEEK_SET);
  write_cmd = is_write;
}

/**
 * Handles SD card commands by processing the given command and performing
 * the corresponding actions. The method uses a switch statement to handle
 * different command codes defined by the MMC (MultiMediaCard) specification.
 * 
 * The method updates the SD card's response registers (`base[SDRSP0]` to `base[SDRSP3]`)
 * based on the command, and may also modify internal state variables such as `read_ext_csd`,
 * `addr`, `blkcnt`, etc. For commands like `MMC_READ_MULTIPLE_BLOCK` and `MMC_WRITE_MULTIPLE_BLOCK`,
 * it prepares the card for read/write operations by calling `prepare_rw`.
 * 
 * If an unrecognized command is provided, the method triggers a panic with an error message
 * indicating the unhandled command.
 * 
 * @param cmd The SD card command to handle, as defined by the MMC specification.
 */
static void sdcard_handle_cmd(int cmd) {
  switch (cmd) {
    case MMC_GO_IDLE_STATE: break;
    case MMC_SEND_OP_COND: base[SDRSP0] = 0x80ff8000; break;
    case MMC_ALL_SEND_CID:
      base[SDRSP0] = 0x00000001;
      base[SDRSP1] = 0x00000000;
      base[SDRSP2] = 0x00000000;
      base[SDRSP3] = 0x15000000;
      break;
    case 52: // ???
      break;
    case MMC_SEND_CSD:
      base[SDRSP0] = 0x92404001;
      base[SDRSP1] = 0x124b97e3 | ((C_SIZE & 0x3) << 30);
      base[SDRSP2] = 0x0f508000 | (C_SIZE >> 2) | (READ_BL_LEN << 16);
      base[SDRSP3] = 0x9026012a;
      break;
    case MMC_SEND_EXT_CSD: read_ext_csd = true; addr = 0; break;
    case MMC_SLEEP_AWAKE: break;
    case MMC_APP_CMD: break;
    case MMC_SET_RELATIVE_ADDR: break;
    case MMC_SELECT_CARD: break;
    case MMC_SET_BLOCK_COUNT: blkcnt = base[SDARG] & 0xffff; break;
    case MMC_READ_MULTIPLE_BLOCK: prepare_rw(false); break;
    case MMC_WRITE_MULTIPLE_BLOCK: prepare_rw(true); break;
    case MMC_SEND_STATUS: base[SDRSP0] = 0x900; base[SDRSP1] = base[SDRSP2] = base[SDRSP3] = 0; break;
    case MMC_STOP_TRANSMISSION: break;
    default:
      panic("unhandled command = %d", cmd);
  }
}

/**
 * Handles I/O operations for an SD card emulation.
 *
 * This function processes read and write operations to the SD card's memory-mapped registers.
 * It interprets the provided `offset` to determine which register is being accessed and performs
 * the corresponding operation based on the `is_write` flag and the register type.
 *
 * The function supports the following registers:
 * - `SDCMD`: Handles SD card commands by extracting the command code and delegating to `sdcard_handle_cmd`.
 * - `SDARG`, `SDRSP0`, `SDRSP1`, `SDRSP2`, `SDRSP3`: Placeholder registers that currently do nothing.
 * - `SDDATA`: Manages data transfer operations. If `read_ext_csd` is true, it emulates reading from the
 *   Extended CSD register by returning predefined values. Otherwise, it reads or writes data to/from
 *   a file pointer (`fp`) depending on the `write_cmd` flag. The `addr` variable tracks the current
 *   address for data operations.
 *
 * For unhandled offsets, the function logs the details and triggers a panic to indicate an unsupported
 * operation.
 *
 * @param offset The memory offset indicating the register being accessed.
 * @param len The length of the data being read or written (unused in this implementation).
 * @param is_write A flag indicating whether the operation is a write (true) or read (false).
 */
static void sdcard_io_handler(uint32_t offset, int len, bool is_write) {
  int idx = offset / 4;
  switch (idx) {
    case SDCMD: sdcard_handle_cmd(base[SDCMD] & 0x3f); break;
    case SDARG:
    case SDRSP0:
    case SDRSP1:
    case SDRSP2:
    case SDRSP3:
      break;
    case SDDATA:
       if (read_ext_csd) {
         // See section 8.1 JEDEC Standard JED84-A441
         uint32_t data;
         switch (addr) {
           case 192: data = 2; break; // EXT_CSD_REV
           case 212: data = MEMORY_SIZE / 512; break;
           default: data = 0;
         }
         base[SDDATA] = data;
         if (addr == 512 - 4) read_ext_csd = false;
       } else if (fp) {
         __attribute__((unused)) int ret;
         if (!write_cmd) { ret = fread(&base[SDDATA], 4, 1, fp); }
         else { ret = fwrite(&base[SDDATA], 4, 1, fp); }
       }
       addr += 4;
       break;
    default:
      Log("offset = 0x%x(idx = %d), is_write = %d, data = 0x%x", offset, idx, is_write, base[idx]);
      panic("unhandle offset = %d", offset);
  }
}

/**
 * Initializes the SD card by setting up the necessary memory-mapped I/O (MMIO) 
 * space and opening the SD card image file. This method performs the following steps:
 * 
 * 1. Allocates a new memory space of size 0x80 bytes and assigns it to the `base` pointer.
 * 2. Maps the allocated memory space to the SD card controller's MMIO region using 
 *    the `add_mmio_map` function, specifying the handler `sdcard_io_handler` for I/O operations.
 * 3. Asserts that the `C_SIZE` value fits within 12 bits to ensure compatibility.
 * 4. Attempts to open the SD card image file specified by `CONFIG_SDCARD_IMG_PATH` in 
 *    read-write mode. If the file cannot be opened, an error message is logged.
 * 
 * This method is essential for setting up the SD card interface and ensuring that the 
 * SD card image is accessible for further operations.
 */
void init_sdcard() {
  base = (uint32_t *)new_space(0x80);
  add_mmio_map("sdhci", CONFIG_SDCARD_CTL_MMIO, base, 0x80, sdcard_io_handler);

  Assert(C_SIZE < (1 << 12), "shoule be fit in 12 bits");

  const char *img = CONFIG_SDCARD_IMG_PATH;
  fp = fopen(img, "r+");
  if (fp == NULL) Log("Can not find sdcard image: %s", img);
}
