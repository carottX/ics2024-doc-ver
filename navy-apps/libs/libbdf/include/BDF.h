#ifndef __BDF_H__
#define __BDF_H__

#include <stdint.h>

/**
 * @brief Constructs a BDF_Font object by loading font data from a specified file.
 *
 * This constructor initializes a BDF_Font object by reading font data from a BDF (Glyph Bitmap Distribution Format) file.
 * The file is expected to contain font metrics, glyph bitmaps, and other relevant information. The constructor parses
 * the file, extracts the necessary data, and stores it in the object's internal structures for later use.
 *
 * @param filename The path to the BDF file containing the font data.
 * @throws std::runtime_error If the file cannot be opened or if the file format is invalid.
 */
BDF_Font::BDF_Font(const char *filename) {
  // Implementation details for loading and parsing the BDF file
}

/**
 * @brief Destructor for the BDF_Font object.
 *
 * This destructor cleans up any dynamically allocated memory associated with the BDF_Font object,
 * such as the font glyph bitmaps stored in the `font` array. It ensures that there are no memory
 * leaks when the object is destroyed.
 */
BDF_Font::~BDF_Font() {
  // Implementation details for freeing allocated memory
};

#endif
