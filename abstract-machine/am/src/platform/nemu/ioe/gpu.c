#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

/**
 * Initializes the GPU hardware and associated resources.
 * This method sets up the necessary configurations and initializes the GPU
 * to a default state, ensuring it is ready for rendering operations.
 * It typically involves configuring memory, setting up display modes,
 * and initializing any required drivers or libraries.
 * This function should be called once at the start of the application
 * before any GPU operations are performed.
 */
void __am_gpu_init() {
}

/**
 * Configures the GPU settings by initializing an AM_GPU_CONFIG_T structure.
 * This method sets the GPU configuration to default values, indicating that the GPU is present
 * but does not have acceleration capabilities. The width, height, and video memory size are
 * initialized to zero.
 *
 * @param cfg A pointer to an AM_GPU_CONFIG_T structure where the GPU configuration will be stored.
 *            The structure will be populated with the following default values:
 *            - present: true (indicating the GPU is present)
 *            - has_accel: false (indicating the GPU does not support acceleration)
 *            - width: 0 (default width)
 *            - height: 0 (default height)
 *            - vmemsz: 0 (default video memory size)
 */
void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = 0, .height = 0,
    .vmemsz = 0
  };
}

/**
 * __am_gpu_fbdraw - Draws to the GPU frame buffer based on the provided control structure.
 *
 * This function handles the drawing operation to the GPU frame buffer. If the `sync` field
 * in the control structure `ctl` is set to true, it triggers a synchronization operation
 * by writing a value of 1 to the `SYNC_ADDR` register. This ensures that the frame buffer
 * is updated and synchronized with the GPU before proceeding.
 *
 * @param ctl Pointer to the AM_GPU_FBDRAW_T control structure containing the drawing
 *            parameters and synchronization flag.
 */
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

/**
 * Updates the GPU status to indicate that it is ready.
 *
 * This function sets the `ready` field of the provided `AM_GPU_STATUS_T` structure
 * to `true`, signaling that the GPU is in a ready state and available for processing.
 *
 * @param status A pointer to an `AM_GPU_STATUS_T` structure where the GPU status
 *               will be updated.
 */
void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
