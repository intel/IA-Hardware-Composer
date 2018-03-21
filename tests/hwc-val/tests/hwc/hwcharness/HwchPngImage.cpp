/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "HwchPngImage.h"
#include "HwcTestState.h"
#include "HwchSystem.h"

#include <string>

//////////////////////////////////// PngImage
////////////////////////////////////////////////
Hwch::PngImage::PngImage(const char* filename)
    : mRowPointers(0), mLoaded(false), mpTexture(0) {
  mDataBlob = NULL;

  if (filename) {
    ReadPngFile(filename);
  }
}

Hwch::PngImage::~PngImage() {
  // cleanup heap memory

  if (mRowPointers) {
    delete[] mRowPointers;
  }

  if (mDataBlob) {
    delete mDataBlob;
    mDataBlob = NULL;
  }

  if (mpTexture) {
    Hwch::System::getInstance().GetGl().FreeTexture(mpTexture);
  }
}

static void PreMultiply(png_bytep pixel) {
  uint32_t alpha = pixel[3];
  pixel[0] = (alpha * pixel[0]) / 255;
  pixel[1] = (alpha * pixel[1]) / 255;
  pixel[2] = (alpha * pixel[2]) / 255;
}

//
// ReadPngFile(const char* input_file)
// This routine reads the input file, retrieves the pre-image information and
// stores that
// in the appropriate class image variables. Then allocates the row pointers and
// copies in those the image itself.
//
// Input Parameters:
// input_file = name of the .png file
//
// Returns true on success.
//
bool Hwch::PngImage::ReadPngFile(const char* fileName) {
  // Save the unadulterated name
  // better for identification
  mName = fileName;

  // retrieve file path
  const char* dirPath = getenv("HWCVAL_IMAGE_DIR");

  if (dirPath == 0) {
    mInputFile = fileName;
  } else {
    mInputFile = std::string(dirPath) + std::string("\/") + std::string(fileName);
  }

#ifdef HWCVAL_NO_PNG
  // Create a dummy image without reading the file.
  // Just to see if PNG is the problem...
  mWidth = 256;
  mHeight = 128;
  uint32_t lineLength = mWidth * 4;
  mDataBlob = new uint8_t[lineLength * mHeight];
  mRowPointers = new uint8_t* [mHeight];
  for (uint32_t i = 0; i < mHeight; ++i) {
    mRowPointers[i] = mDataBlob + i * lineLength;
  }
  mColorType = PNG_COLOR_TYPE_RGB_ALPHA;
  mLoaded = true;
  return mLoaded;
// End of dummy stuff
#endif

  PngReader reader;
  mLoaded = reader.Read(mInputFile.c_str(), mRowPointers, mDataBlob, mWidth,
                        mHeight, mColorType, mBitDepth);
  return mLoaded;
}

const char* Hwch::PngImage::GetName() {
  return mName.c_str();
}

bool Hwch::PngImage::IsLoaded() {
  return mLoaded;
}

//
// ProcessFile(void)
// This routine performs a simple transformation on the row pointers.
//
// Input Parameters: none
//
// Return Values:
// 0 = success/1 = failure
//
bool Hwch::PngImage::ProcessFile(void) {
  bool bRet = false;

  // Only suitable for RGB
  for (uint32_t row = 0; row < mHeight; row++) {
    png_byte* pRow = mRowPointers[row];

    for (uint32_t column = 0; column < mWidth; column++) {
      png_byte* ptr = &(pRow[column * 4]);

      // printf("Pixel at position [ %d - %d ] has RGBA values: %d - %d - %d -
      // %d\n",
      //       column, row, ptr[0], ptr[1], ptr[2], ptr[3]);

      // set red value to 0 and green value to the blue one
      ptr[0] = 0;
      ptr[1] = ptr[2];
    }
  }

  return bRet;
}

// Return image in GL-friendly form
Hwch::TexturePtr Hwch::PngImage::GetTexture() {
  if (mpTexture == 0) {
    mpTexture = Hwch::System::getInstance().GetGl().LoadTexture(*this);
  }

  return mpTexture;
}

///////////////////////////////////// PngReader ///////////////////////////
Hwch::PngReader::PngReader() : mpPngStruct(0), mpPngInfo(0), mFp(0) {
}

bool Hwch::PngReader::Read(const char* path, png_bytep*& rowPointers,
                           uint8_t*& dataBlob, uint32_t& width,
                           uint32_t& height, uint32_t& colourType,
                           uint32_t& bitDepth) {
  png_byte header[8];
  uint32_t num_channels;
  uint32_t num_passes;

  // open file and test if png
  mFp = fopen(path, "rb");
  if (!mFp) {
    fprintf(stderr,
            "ERROR: No such file %s.\n"
            "Please ensure image files are on the target and that path of "
            "image directory is given by environment variable "
            "HWCVAL_IMAGE_DIR.\n",
            path);
    HWCERROR(eCheckFileError, "File %s could not be opened for reading", path);
    return false;
  }

  fread(header, 1, 8, mFp);
  if (png_sig_cmp(header, 0, 8)) {
    HWCERROR(eCheckFileError, "File %s is not recognized as a PNG file", path);
    return false;
  }

  // initializations

  mpPngStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!mpPngStruct) {
    HWCERROR(eCheckInternalError, "png_create_read_struct failed");
    return false;
  }

  mpPngInfo = png_create_info_struct(mpPngStruct);
  if (!mpPngInfo) {
    HWCERROR(eCheckInternalError, "png_create_info_struct failed");
    return false;
  }

  // NOTE: When libpng encounters an error, it expects to longjmp back to the
  // routine.
  // -> need to call setjmp and pass the jmpbuf field of the png_struct.
  // As the file is read from different routines the jmpbuf field must be
  // updated
  // every time a routine calls a png_ function.
  if (setjmp(png_jmpbuf(mpPngStruct))) {
    HWCERROR(eCheckInternalError, "Error during initialization");
    return false;
  }
  png_init_io(mpPngStruct, mFp);
  png_set_sig_bytes(mpPngStruct, 8);

  // retrieve pre-image information

  png_read_info(mpPngStruct, mpPngInfo);
  mWidth = png_get_image_width(mpPngStruct, mpPngInfo);
  mHeight = png_get_image_height(mpPngStruct, mpPngInfo);
  mColorType = png_get_color_type(mpPngStruct, mpPngInfo);
  mBitDepth = png_get_bit_depth(mpPngStruct, mpPngInfo);

  if (mColorType == PNG_COLOR_TYPE_RGB_ALPHA) {
    num_channels = 4;
  } else {
    // TODO: consider the others

    HWCERROR(eCheckPngFail,
             "Input file must be PNG_COLOR_TYPE_RGBA; mColorType=%d not %d",
             mColorType, PNG_COLOR_TYPE_RGB_ALPHA);
    return false;
  }

  mBytesPerPixel = (mBitDepth * num_channels) / 8;
  mBytesPerRow = png_get_rowbytes(mpPngStruct, mpPngInfo);
  num_passes = png_set_interlace_handling(mpPngStruct);
  png_read_update_info(mpPngStruct, mpPngInfo);

  // read image

  if (setjmp(png_jmpbuf(mpPngStruct))) {
    HWCERROR(eCheckPngFail, "Error during read_image");
    return false;
  }

  uint32_t rowBytes = png_get_rowbytes(mpPngStruct, mpPngInfo);
  dataBlob = new uint8_t[mHeight * rowBytes];

  rowPointers = new png_bytep[mHeight]();
  for (uint32_t row = 0; row < mHeight; row++) {
    rowPointers[row] = (png_bytep)(dataBlob + (row * rowBytes));
  }

  png_read_image(mpPngStruct, rowPointers);

  // Perform premultiplication
  for (uint32_t row = 0; row < mHeight; ++row) {
    png_bytep line = rowPointers[row];

    for (uint32_t i = 0; i < mWidth * 4; i += 4) {
      PreMultiply(line + i);
    }
  }

  width = mWidth;
  height = mHeight;
  colourType = mColorType;
  bitDepth = mBitDepth;

  return true;
}

/*
//
// WritePngFile(const char* output_file)
// This routine writes in a output file a png image header and the image itself
// contained in the row pointers.
//
// Input Parameters:
// output_file = name of the .png file created
//
// Return Values:
// 0 = success/1 = failure
//
bool Hwch::PngImage::WritePngFile(const char* fileName)
{
    bool bRet = false;

    FILE *fp = fopen(fileName, "wb");
    if (!fp)
    {
        HWCLOGE("File %s could not be opened for writing", fileName);
        PngCleanUp(&bRet, &fp);
    }


    // initializations

    mpPngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
NULL);
    if (!mpPngStruct)
    {
        HWCLOGE("png_create_write_struct failed");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }

    mpPngInfo = png_create_info_struct(mpPngStruct);
    if (!mpPngInfo)
    {
        HWCLOGE("png_create_info_struct failed");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }

    if (setjmp(png_jmpbuf(mpPngStruct)))
    {
        HWCLOGE("Error during initialization");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }
    png_init_io(mpPngStruct, fp);


    // write image header

    if (setjmp(png_jmpbuf(mpPngStruct)))
    {
        HWCLOGE("Error during writing header");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }
    png_set_IHDR(mpPngStruct, mpPngInfo, mWidth, mHeight, mBitDepth, mColorType,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
PNG_FILTER_TYPE_BASE);
    png_write_info(mpPngStruct, mpPngInfo);


    // write image

    if (setjmp(png_jmpbuf(mpPngStruct)))
    {
        HWCLOGE("Error during writing bytes");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }
    png_write_image(mpPngStruct, mRowPointers);

    if (setjmp(png_jmpbuf(mpPngStruct)))
    {
        HWCLOGE("Error during end of write");
        PngCleanUp(&bRet, &fp);
        return bRet;
    }
    png_write_end(mpPngStruct, NULL);

    fclose(fp);

    return bRet;
}
*/

//
// Destructor
// Perform tidyup of PNG structures.
//
Hwch::PngReader::~PngReader() {
  if (mFp != NULL)
    fclose(mFp);
  if (mpPngStruct != NULL)
    png_destroy_read_struct(&mpPngStruct, &mpPngInfo, NULL);
}
