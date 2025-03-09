#include <am.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define BLKSZ 512

static int disk_size = 0;
static FILE *fp = NULL;

/**
 * Initializes the disk by opening the disk image file specified in the environment variable "diskimg".
 * If the environment variable is set and the file is successfully opened, the method calculates the size
 * of the disk in terms of 512-byte sectors. The file pointer is then rewound to the beginning of the file
 * for subsequent operations.
 *
 * The disk size is calculated by seeking to the end of the file, determining the file size in bytes,
 * and then converting this size to the number of 512-byte sectors, rounding up if necessary.
 *
 * If the environment variable "diskimg" is not set or the file cannot be opened, the method does nothing.
 *
 * @note The global variable `fp` is used to store the file pointer to the disk image.
 * @note The global variable `disk_size` is updated with the calculated size of the disk in sectors.
 */
void __am_disk_init() {
  const char *diskimg = getenv("diskimg");
  if (diskimg) {
    fp = fopen(diskimg, "r+");
    if (fp) {
      fseek(fp, 0, SEEK_END);
      disk_size = (ftell(fp) + 511) / 512;
      rewind(fp);
    }
  }
}

/**
 * Configures the disk settings by populating the provided AM_DISK_CONFIG_T structure.
 * 
 * This function sets the following fields in the AM_DISK_CONFIG_T structure:
 * - `present`: Indicates whether the disk is present by checking if the file pointer `fp` is not NULL.
 * - `blksz`: Sets the block size to the predefined value `BLKSZ`.
 * - `blkcnt`: Sets the block count to the value of `disk_size`.
 * 
 * @param cfg Pointer to the AM_DISK_CONFIG_T structure to be populated with disk configuration details.
 */
void __am_disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->present = (fp != NULL);
  cfg->blksz = BLKSZ;
  cfg->blkcnt = disk_size;
}

/**
 * @brief Updates the disk status to indicate that the disk is ready.
 *
 * This function sets the `ready` field of the `AM_DISK_STATUS_T` structure to 1,
 * indicating that the disk is in a ready state and available for operations.
 *
 * @param stat Pointer to an `AM_DISK_STATUS_T` structure where the disk status
 *             will be updated.
 */
void __am_disk_status(AM_DISK_STATUS_T *stat) {
  stat->ready = 1;
}

/**
 * Performs block I/O operations on a disk file.
 *
 * This function reads from or writes to a disk file based on the provided I/O request.
 * The file pointer `fp` must be valid and open for the operation to proceed. The function
 * seeks to the specified block number (`blkno`) multiplied by the block size (`BLKSZ`) in the file.
 * If the I/O request is a write operation (`io->write` is true), the function writes the data from
 * the buffer (`io->buf`) to the file. If it is a read operation, the function reads data into the buffer.
 * The number of blocks to read or write is specified by `blkcnt`. The function asserts that the
 * operation completes successfully by checking the return value of `fread` or `fwrite`.
 *
 * @param io Pointer to an `AM_DISK_BLKIO_T` structure containing the I/O request details.
 *           The structure must include the following fields:
 *           - `blkno`: The block number to start the I/O operation.
 *           - `blkcnt`: The number of blocks to read or write.
 *           - `buf`: The buffer containing data to write or to store read data.
 *           - `write`: A boolean flag indicating whether the operation is a write (true) or read (false).
 */
void __am_disk_blkio(AM_DISK_BLKIO_T *io) {
  if (fp) {
    fseek(fp, io->blkno * BLKSZ, SEEK_SET);
    int ret;
    if (io->write) ret = fwrite(io->buf, io->blkcnt * BLKSZ, 1, fp);
    else ret = fread(io->buf, io->blkcnt * BLKSZ, 1, fp);
    assert(ret == 1);
  }
}
