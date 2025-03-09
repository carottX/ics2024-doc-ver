#include <am.h>
#include <nemu.h>

/**
 * Configures the disk status by initializing the provided AM_DISK_CONFIG_T structure.
 * This function sets the 'present' field of the structure to 'false', indicating that
 * no disk is currently present or configured. This can be used to reset or initialize
 * the disk configuration state before further operations.
 *
 * @param cfg Pointer to an AM_DISK_CONFIG_T structure that will be configured.
 */
void __am_disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->present = false;
}

/**
 * @brief Retrieves the current status of the disk.
 *
 * This method populates the provided `AM_DISK_STATUS_T` structure with the 
 * current status information of the disk. The status may include details such 
 * as whether the disk is ready, the number of pending operations, or any 
 * error conditions that have been encountered.
 *
 * @param stat A pointer to an `AM_DISK_STATUS_T` structure where the disk 
 *             status information will be stored. The structure must be 
 *             allocated by the caller.
 *
 * @note The caller is responsible for ensuring that the `stat` pointer is 
 *       valid and that the structure is properly initialized before calling 
 *       this method.
 */
void __am_disk_status(AM_DISK_STATUS_T *stat) {
}

/**
 * Performs block I/O operations on a disk as specified by the provided AM_DISK_BLKIO_T structure.
 * This function is responsible for handling read and write operations to the disk, managing
 * data transfer between the disk and memory, and ensuring the integrity of the operations.
 * 
 * @param io Pointer to an AM_DISK_BLKIO_T structure that contains the details of the I/O operation,
 *           including the type of operation (read/write), the disk sector to access, the memory
 *           buffer for data transfer, and the size of the data to be transferred.
 * 
 * @note The function assumes that the AM_DISK_BLKIO_T structure is properly initialized and that
 *       the memory buffer is allocated and accessible. The caller is responsible for ensuring
 *       that the structure and buffer are valid before invoking this function.
 */
void __am_disk_blkio(AM_DISK_BLKIO_T *io) {
}
