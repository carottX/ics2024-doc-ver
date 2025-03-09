#include <stdio.h>
#include <assert.h>

/**
 * The main function performs a series of operations on a file to verify and modify its contents.
 * 
 * The file "/share/files/num" is opened in read-write mode. The function first checks that the file size is exactly 5000 bytes.
 * It then seeks to the middle of the file (byte 2500) and reads 500 integers, asserting that each integer matches the expected value (501 to 1000).
 * 
 * Next, the function seeks back to the beginning of the file and writes 500 integers starting from 1001 to 1500, each formatted as a 4-digit number followed by a newline.
 * 
 * After writing, the function seeks to the middle of the file again and reads the same 500 integers, asserting that they remain unchanged (501 to 1000).
 * 
 * Finally, the function seeks back to the beginning of the file and reads the first 500 integers, asserting that they now match the modified values (1001 to 1500).
 * 
 * If all assertions pass, the function prints "PASS!!!" and returns 0.
 */
int main() {
  FILE *fp = fopen("/share/files/num", "r+");
  assert(fp);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  assert(size == 5000);

  fseek(fp, 500 * 5, SEEK_SET);
  int i, n;
  for (i = 500; i < 1000; i ++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1);
  }

  fseek(fp, 0, SEEK_SET);
  for (i = 0; i < 500; i ++) {
    fprintf(fp, "%4d\n", i + 1 + 1000);
  }

  for (i = 500; i < 1000; i ++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1);
  }

  fseek(fp, 0, SEEK_SET);
  for (i = 0; i < 500; i ++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1 + 1000);
  }

  fclose(fp);

  printf("PASS!!!\n");

  return 0;
}
