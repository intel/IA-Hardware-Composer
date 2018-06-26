#include <stdio.h>
#include <stdlib.h>

#define LICENSE_TERM                                                         \
  "// Copyright (c) 2018 Intel Corporation\n"                                \
  "//\n"                                                                     \
  "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"   \
  "// you may not use this file except in compliance with the License.\n"    \
  "// You may obtain a copy of the License at\n"                             \
  "//\n"                                                                     \
  "//      http://www.apache.org/licenses/LICENSE-2.0\n"                     \
  "//\n"                                                                     \
  "// Unless required by applicable law or agreed to in writing, software\n" \
  "// distributed under the License is distributed on an \"AS IS\" BASIS,\n" \
  "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or "      \
  "implied.\n"                                                               \
  "// See the License for the specific language governing permissions and\n" \
  "// limitations under the License.\n"                                      \
  "\n"                                                                       \
  "// clang-format off\n"

void print_usage(void) {
  fprintf(stdout, "./bin_to_c_array <input bin file> <output file>\n");
  fprintf(stdout,
          "ex)./bin_to_c_array hwc_shader_prog_1.shader_test.bin "
          "hwc_shader_prog_1.h\n\n");
}

int main(int argc, char *argv[]) {
  FILE *binary_fp, *header_fp;
  int byte;
  int i = 0;

  if (argc < 3) {
    fprintf(stderr, "need input and output file names\n");
    print_usage();
    exit(-1);
  }

  /* print out information */
  fprintf(stdout, "input file:%s\n", argv[1]);
  fprintf(stdout, "output file:%s\n", argv[2]);

  binary_fp = fopen(argv[1], "rb");

  if (!binary_fp) {
    fprintf(stderr, "input file does not exist\n");
    exit(-1);
  }

  /* getting the size of binary file */
  fseek(binary_fp, 0, SEEK_END);
  long binary_sz = ftell(binary_fp);
  rewind(binary_fp);

  if (binary_sz == 0) {
    fprintf(stderr, "binary size is 0.. no data to retrieve..exiting.\n");
    fclose(binary_fp);
    exit(-1);
  }

  header_fp = fopen(argv[2], "wb");

  if (!header_fp) {
    fprintf(stderr, "failed to create output file\n");
    fclose(binary_fp);
    exit(-1);
  }

  fprintf(header_fp, LICENSE_TERM "\n");

  fprintf(stdout, "file size: %ld\n", binary_sz);

  /* write binary size to the beginning of the file */
  for (i = 0; i < sizeof(binary_sz); i++) {
    fprintf(header_fp, "0x%02x, ", (unsigned char)(binary_sz & 0xFF));
    binary_sz >>= 8;
  }

  fprintf(header_fp, "// size of this binary\n");

  while (byte != EOF) {
    for (i = 0; i < 12; i++) {
      byte = fgetc(binary_fp);
      fprintf(header_fp, "0x%02x, ", (unsigned char)byte);
    }
    fprintf(header_fp, "\n");
  }

  fprintf(stdout,
          "Done, file %s has been successfully"
          " generated\n",
          argv[2]);

  fclose(header_fp);
  fclose(binary_fp);

  return 0;
}
