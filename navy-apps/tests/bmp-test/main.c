#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <NDL.h>
#include <BMP.h>

/**
 * The main function initializes the NDL (Native Display Library) and loads a BMP image
 * from the specified path. It retrieves the width and height of the image and opens
 * a canvas with the same dimensions. The image is then drawn onto the canvas. After
 * rendering, the image data is freed, and the NDL is terminated. The function prints
 * a message indicating the end of the test and enters an infinite loop to keep the
 * program running. This function serves as a simple test for displaying a BMP image
 * using the NDL library.
 *
 * @return int - Returns 0 upon successful execution.
 */
int main() {
  NDL_Init(0);
  int w, h;
  void *bmp = BMP_Load("/share/pictures/projectn.bmp", &w, &h);
  assert(bmp);
  NDL_OpenCanvas(&w, &h);
  NDL_DrawRect(bmp, 0, 0, w, h);
  free(bmp);
  NDL_Quit();
  printf("Test ends! Spinning...\n");
  while (1);
  return 0;
}
