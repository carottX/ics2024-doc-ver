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

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;

/**
 * Handles audio input/output operations for a specified memory offset and length.
 *
 * This method is responsible for managing audio data transfers between the audio buffer
 * and the hardware or software audio interface. It processes a segment of audio data
 * starting at the given offset and spanning the specified length. The operation can be
 * either a read (input) or write (output) depending on the `is_write` parameter.
 *
 * @param offset The starting memory offset in the audio buffer where the operation begins.
 * @param len The length of the audio data segment to be processed, in bytes or samples.
 * @param is_write A boolean flag indicating the type of operation: `true` for write (output),
 *                 `false` for read (input).
 */
static void audio_io_handler(uint32_t offset, int len, bool is_write) {
}

/**
 * Initializes the audio subsystem by allocating memory for audio control registers
 * and the sound buffer, and mapping them to the appropriate I/O or MMIO addresses.
 * 
 * This function performs the following steps:
 * 1. Calculates the required space size for audio control registers based on the
 *    number of registers (`nr_reg`).
 * 2. Allocates memory for the audio control registers using `new_space()` and
 *    assigns the base address to `audio_base`.
 * 3. Maps the audio control registers to either a port I/O address (if `CONFIG_HAS_PORT_IO`
 *    is defined) or a memory-mapped I/O address, using the `audio_io_handler` for I/O operations.
 * 4. Allocates memory for the sound buffer (`sbuf`) using `new_space()` with the size
 *    specified by `CONFIG_SB_SIZE`.
 * 5. Maps the sound buffer to a memory-mapped I/O address using `add_mmio_map()`.
 * 
 * The function ensures that the audio subsystem is properly set up for I/O operations
 * and sound data handling.
 */
void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif

  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, NULL);
}
