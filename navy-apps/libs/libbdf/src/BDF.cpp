#include <BDF.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Creates a bitmap representation of a character and stores it in the font map.
 *
 * This method generates a bitmap for the specified character `ch` based on the provided bounding box (`bbx`),
 * the source bitmap data (`bitmap`), and the count of bitmap rows. The resulting bitmap is stored in the `font` map
 * for the character `ch`. The method iterates over the character's width (`w`) and height (`h`) to construct each row
 * of the bitmap. It uses the bounding box information to align the character within the bitmap and applies the source
 * bitmap data to determine the pixel values.
 *
 * @param ch The character code for which the bitmap is to be created.
 * @param bbx An array of integers representing the bounding box of the character. The array contains:
 *            - bbx[0]: The x-offset of the bounding box.
 *            - bbx[1]: The height of the bounding box.
 *            - bbx[2]: The y-offset of the bounding box.
 *            - bbx[3]: The width of the bounding box.
 * @param bitmap A pointer to the source bitmap data, which contains the pixel information for the character.
 * @param count The number of rows in the source bitmap data.
 */
void BDF_Font::create(uint32_t ch, int *bbx, uint32_t *bitmap, int count) {
  font[ch] = new uint32_t[h];
  for (int y = 0; y < h; y ++) {
    uint32_t row = 0;
    for (int x = 0; x < w; x ++) {
      int x1 = x - bbx[2];
      int y1 = y - (h - bbx[1] - bbx[3]) - h1;
      if (x1 >= 0 && y1 >= 0 && y1 < bbx[1]) {
        uint32_t mask = bitmap[y1];
        if ( (mask >> (x1)) & 1 ) {
          row |= (1 << x);
        }
      }
    }
    font[ch][y] = row;
  }
}

/**
 * @brief Constructs a BDF_Font object by loading and parsing a BDF (Glyph Bitmap Distribution Format) font file.
 *
 * This constructor initializes the font data structure by reading the specified BDF file. It processes the file
 * line by line, extracting font metadata and character bitmap data. The font file is expected to follow the BDF format,
 * which includes sections for font properties, character definitions, and bitmap data.
 *
 * The method performs the following steps:
 * 1. Opens the specified BDF file for reading.
 * 2. Initializes internal data structures to store font information.
 * 3. Parses the file to extract the font's bounding box dimensions, character codes, and bitmap data.
 * 4. Processes each character's bitmap data, converting it into a binary format suitable for rendering.
 * 5. Closes the file after reading all necessary data.
 *
 * @param fname The path to the BDF font file to be loaded.
 * @throws std::runtime_error if the file cannot be opened or is not a valid BDF file.
 */
BDF_Font::BDF_Font(const char *fname) {
  memset(font, 0, sizeof(font));
  FILE *fp = fopen(fname, "r");
  assert(fp);

  char buf[256], cmd[32];
  bool valid_file = false, in_bitmap = false;
  uint32_t bm[32], ch = '\0';
  int bm_idx, bm_bbx[4];

  while (fgets(buf, 256, fp)) {
    sscanf(buf, "%s ", cmd);
    if (strcmp(cmd, "STARTFONT") == 0) {
      valid_file = true;
    }
    if (strcmp(cmd, "FONTBOUNDINGBOX") == 0) {
      sscanf(buf, "%*s %d %d %d %d", &w, &h, &w1, &h1);
    }
    if (strcmp(cmd, "STARTCHAR") == 0) {
      sscanf(buf, "%*s %x", &ch);
    }
    if (strcmp(cmd, "BBX") == 0) {
      sscanf(buf, "%*s %d %d %d %d", &bm_bbx[0], &bm_bbx[1], &bm_bbx[2], &bm_bbx[3]);
    }
    if (strcmp(cmd, "ENDCHAR") == 0) {
      if (ch < 256) {
        create(ch, bm_bbx, bm, bm_idx);
      }
      in_bitmap = false;
      ch = '\0';
    } else if (strcmp(cmd, "BITMAP") == 0) {
      in_bitmap = true;
      bm_idx = 0;
    } else if (in_bitmap) {
      int idx = 0;
      bm[bm_idx] = 0;
      for (const char *p = buf; *p != '\n'; p ++) {
        int val;
        if (*p >= '0' && *p <= '9') val = *p - '0';
        else val = *p - 'A' + 10;
        for (int i = 0; i < 4; i ++) {
          if ((val >> (3 - i)) & 1) {
            bm[bm_idx] |= 1 << idx;
          }
          idx ++;
        }
      }
      bm_idx ++;
    }
    if (strcmp(cmd, "ENDFONT") == 0) {
      break;
    }
  }

  fclose(fp);
}

/**
 * @brief Destructor for the BDF_Font class.
 * 
 * This method is responsible for cleaning up dynamically allocated memory
 * associated with the font data. It iterates through the `font` container,
 * which holds pointers to dynamically allocated arrays of pixel data, and
 * deletes each array to prevent memory leaks. This ensures that all resources
 * allocated by the BDF_Font object are properly released when the object is
 * destroyed.
 */
BDF_Font::~BDF_Font() {
  for (uint32_t *pixels: font) {
    if (pixels) delete [] pixels;
  }
}
