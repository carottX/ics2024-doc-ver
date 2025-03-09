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

#define SCREEN_W (MUXDEF(CONFIG_VGA_SIZE_800x600, 800, 400))
#define SCREEN_H (MUXDEF(CONFIG_VGA_SIZE_800x600, 600, 300))

/**
 * Retrieves the width of the screen based on the current configuration.
 * 
 * This method returns the screen width by checking the configuration target.
 * If the target is set to AM (Abstract Machine), it reads the width from the 
 * AM GPU configuration. Otherwise, it returns the default screen width defined 
 * by `SCREEN_W`.
 * 
 * @return uint32_t The width of the screen in pixels.
 */
static uint32_t screen_width() {
  return MUXDEF(CONFIG_TARGET_AM, io_read(AM_GPU_CONFIG).width, SCREEN_W);
}

/**
 * Retrieves the height of the screen based on the current configuration.
 *
 * This method returns the height of the screen in pixels. The value is determined
 * by the target environment:
 * - If the target is AM (Abstract Machine), it reads the height from the GPU configuration
 *   using the `io_read` function.
 * - Otherwise, it returns a predefined constant `SCREEN_H` which represents the
 *   screen height in non-AM environments.
 *
 * @return The height of the screen as a 32-bit unsigned integer.
 */
static uint32_t screen_height() {
  return MUXDEF(CONFIG_TARGET_AM, io_read(AM_GPU_CONFIG).height, SCREEN_H);
}

/**
 * Calculates the total size of the screen in bytes.
 * 
 * This method computes the screen size by multiplying the screen width, screen height,
 * and the size of a single pixel in bytes (assuming each pixel is represented by a uint32_t).
 * The result represents the total memory required to store the entire screen content.
 * 
 * @return The total size of the screen in bytes as a uint32_t value.
 */
static uint32_t screen_size() {
  return screen_width() * screen_height() * sizeof(uint32_t);
}

static void *vmem = NULL;
static uint32_t *vgactl_port_base = NULL;

#ifdef CONFIG_VGA_SHOW_SCREEN
#ifndef CONFIG_TARGET_AM
#include <SDL2/SDL.h>

static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

/**
 * Initializes the screen for the NEMU (NJU Emulator) application.
 * This function sets up the SDL (Simple DirectMedia Layer) window and renderer,
 * configures the window title based on the guest ISA (Instruction Set Architecture),
 * and creates a texture for rendering. The screen dimensions are scaled based on
 * the configuration (e.g., 2x scaling for 400x300 VGA size). The function performs
 * the following steps:
 * 1. Constructs the window title by appending "-NEMU" to the guest ISA string.
 * 2. Initializes the SDL video subsystem.
 * 3. Creates an SDL window and renderer with dimensions scaled according to the
 *    configuration.
 * 4. Sets the constructed title for the SDL window.
 * 5. Creates an ARGB8888 format texture for rendering.
 * 6. Presents the renderer to display the initialized screen.
 */
static void init_screen() {
  SDL_Window *window = NULL;
  char title[128];
  sprintf(title, "%s-NEMU", str(__GUEST_ISA__));
  SDL_Init(SDL_INIT_VIDEO);
  SDL_CreateWindowAndRenderer(
      SCREEN_W * (MUXDEF(CONFIG_VGA_SIZE_400x300, 2, 1)),
      SCREEN_H * (MUXDEF(CONFIG_VGA_SIZE_400x300, 2, 1)),
      0, &window, &renderer);
  SDL_SetWindowTitle(window, title);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STATIC, SCREEN_W, SCREEN_H);
  SDL_RenderPresent(renderer);
}

/**
 * Updates the screen by rendering the current frame stored in the video memory (vmem) to the display.
 * This function performs the following steps:
 * 1. Updates the texture with the pixel data from the video memory (vmem).
 * 2. Clears the renderer to prepare for the new frame.
 * 3. Copies the updated texture to the renderer.
 * 4. Presents the renderer's contents to the screen, making the new frame visible.
 * 
 * The function assumes that `texture`, `renderer`, and `vmem` are properly initialized and that
 * `SCREEN_W` represents the width of the screen in pixels.
 */
static inline void update_screen() {
  SDL_UpdateTexture(texture, NULL, vmem, SCREEN_W * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}
#else
/**
 * Initializes the screen for display. This function sets up the necessary
 * hardware and software components to prepare the screen for rendering.
 * It typically involves configuring display buffers, setting up the screen
 * resolution, and initializing any required graphical subsystems. This
 * function should be called before any rendering operations are performed.
 */
static void init_screen() {}

/**
 * Updates the screen by drawing the contents of the video memory (vmem) to the display.
 * This method uses the `io_write` function to send a frame buffer draw command to the GPU.
 * The frame buffer is drawn starting at the top-left corner (0, 0) of the screen, 
 * using the entire width and height of the screen as specified by `screen_width()` and `screen_height()`.
 * The `true` parameter indicates that the frame buffer should be immediately synchronized with the display.
 */
static inline void update_screen() {
  io_write(AM_GPU_FBDRAW, 0, 0, vmem, screen_width(), screen_height(), true);
}
#endif
#endif

/**
 * Updates the VGA screen based on the sync register's state.
 *
 * This method checks the value of the sync register. If the sync register is non-zero,
 * it calls the `update_screen()` function to refresh the screen display. After updating
 * the screen, the sync register is reset to zero to indicate that the screen has been
 * updated and to prevent redundant updates.
 *
 * @note Ensure that the sync register is properly initialized and managed by other
 * parts of the system to avoid unintended behavior.
 */
void vga_update_screen() {
  // TODO: call `update_screen()` when the sync register is non-zero,
  // then zero out the sync register
}

/**
 * Initializes the VGA (Video Graphics Array) controller and video memory.
 * This function performs the following operations:
 * 1. Allocates memory for the VGA control port base and sets its initial value
 *    to a combination of the screen width and height.
 * 2. Maps the VGA control port to either I/O port space or memory-mapped I/O
 *    space, depending on the configuration (CONFIG_HAS_PORT_IO).
 * 3. Allocates memory for the video memory (vmem) and maps it to the specified
 *    memory-mapped I/O address (CONFIG_FB_ADDR).
 * 4. If CONFIG_VGA_SHOW_SCREEN is defined, initializes the screen and clears
 *    the video memory by setting it to zero.
 */
void init_vga() {
  vgactl_port_base = (uint32_t *)new_space(8);
  vgactl_port_base[0] = (screen_width() << 16) | screen_height();
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("vgactl", CONFIG_VGA_CTL_PORT, vgactl_port_base, 8, NULL);
#else
  add_mmio_map("vgactl", CONFIG_VGA_CTL_MMIO, vgactl_port_base, 8, NULL);
#endif

  vmem = new_space(screen_size());
  add_mmio_map("vmem", CONFIG_FB_ADDR, vmem, screen_size(), NULL);
  IFDEF(CONFIG_VGA_SHOW_SCREEN, init_screen());
  IFDEF(CONFIG_VGA_SHOW_SCREEN, memset(vmem, 0, screen_size()));
}
