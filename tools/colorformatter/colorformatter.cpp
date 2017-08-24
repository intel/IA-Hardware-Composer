/*
// Copyright (c) 2016 Intel Corporation
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

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#define INPUT_FORMAT_NUM 20
#define OUTPUT_FORMAT_NUM 20

FILE *input_fd = NULL;
FILE *output_fd = NULL;
char *output_buf = NULL;
unsigned long output_buf_size = 0;

struct output_input_formats {
  char output_format[64];
  char input_format_list[INPUT_FORMAT_NUM][64];
};

struct output_input_formats output_input_mapping[OUTPUT_FORMAT_NUM] = {
    {"yv12", {"yuv420p"}},
    {"y8", {"yuv420p"}},
    {"ycbcr_444_888", {"yuv444p"}},
    {"ycbcr_422_i", {"yuyv422"}},
    {"ycbcr_422_sp", {"yuv422p"}},
    {"ycbcr_422_888", {"yuv422p"}},
    {"ycbcr_420_888", {"nv12"}},
    {"ycrcb_420_sp", {"nv21"}},
    {"nv12_linear_cam_intel", {"nv12"}},
    {"nv12_y_tiled_intel", {"nv12"}},
    {"raw10", {"*"}},
    {"raw12", {"*"}},
    {"raw16", {"*"}},
    {"rawopaque", {"*"}},
    {"rawblob", {"*"}},
    {"y16", {"yuv420p16le"}}};

char input_raw[1024];
char output_raw[1024];
char input_format[64];
char output_format[64];
unsigned int width = 0;
unsigned int height = 0;

static void print_help(void) {
  printf(
      "Usage: colorformatter [--help] [--input] [--input-format]"
      "[--width] [--height] [--output] [--output-format]\n");
  printf("\nSupported mapping of output format and input format as below:\n\n");
  printf("\t%-32s%-32s\n", "Output Format", "Input Format");
  printf(
      "---------------------------------------------------------------------"
      "\n");
  for (int i_output = 0; i_output < OUTPUT_FORMAT_NUM; i_output++) {
    if (strlen(output_input_mapping[i_output].output_format) != 0) {
      printf("\t%-32s", output_input_mapping[i_output].output_format);
      for (int i_input = 0; i_input < INPUT_FORMAT_NUM; i_input++) {
        if (strlen(output_input_mapping[i_output].input_format_list[i_input]) !=
            0) {
          printf("%-16s",
                 output_input_mapping[i_output].input_format_list[i_input]);
        }
      }
      printf("\n");
    };
  };
}

static void parse_args(int argc, char *argv[]) {
  static const struct option longopts[] = {
      {"help", no_argument, NULL, '0'},
      {"input", required_argument, NULL, '1'},
      {"input-format", required_argument, NULL, '2'},
      {"output", required_argument, NULL, '3'},
      {"output-format", required_argument, NULL, '4'},
      {"width", required_argument, NULL, '5'},
      {"height", required_argument, NULL, '6'},
      {0},
  };

  char *endptr;
  int opt;
  int longindex = 0;

  opterr = 0;
  memset(input_raw, 0, sizeof(input_raw));
  memset(input_format, 0, sizeof(input_format));
  memset(output_raw, 0, sizeof(output_raw));
  memset(output_format, 0, sizeof(output_format));

  if (argc <= 1) {
    print_help();
    exit(0);
  }

  while ((opt = getopt_long(argc, argv, "+:h0:1:2:3:4:5:6:", longopts,
                            &longindex)) != -1) {
    switch (opt) {
      case '0':
        print_help();
        exit(0);
        break;
      case '1':
        if (strlen(optarg) >= 1024) {
          printf("too long input path, litmited less than 1024!\n");
          exit(0);
        }
        strcpy(input_raw, optarg);
        break;
      case '2':
        if (strlen(optarg) >= 64) {
          printf("too long input format, litmited less than 256!\n");
          exit(0);
        }
        strcpy(input_format, optarg);
        break;
      case '3':
        if (strlen(optarg) >= 1024) {
          printf("too long output path, litmited less than 1024!\n");
          exit(0);
        }
        strcpy(output_raw, optarg);
        break;
      case '4':
        if (strlen(optarg) >= 64) {
          printf("too long output format, litmited less than 256!\n");
          exit(0);
        }
        strcpy(output_format, optarg);
        break;
      case '5':
        errno = 0;
        width = strtoul(optarg, &endptr, 0);
        if (errno || *endptr != '\0') {
          fprintf(stderr, "usage error: invalid value for <width>\n");
          exit(EXIT_FAILURE);
        }
        break;
      case '6':
        errno = 0;
        height = strtoul(optarg, &endptr, 0);
        if (errno || *endptr != '\0') {
          fprintf(stderr, "usage error: invalid value for <height>\n");
          exit(EXIT_FAILURE);
        }
        break;
      default:
        assert(opt == '?');
        fprintf(stderr, "usage error: unknown option '%s'\n", argv[optind - 1]);
        exit(EXIT_FAILURE);
        break;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "usage error: trailing args\n");
    exit(EXIT_FAILURE);
  }
}

bool generate_raw16_output_buf() {
  output_buf_size = width * 2 * height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  return true;
}

bool generate_rawopaque_output_buf() {
  output_buf_size = width * 2 * height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  return true;
}

bool generate_rawblob_output_buf() {
  output_buf_size = width * height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  return true;
}

bool generate_raw10_output_buf() {
  if (width % 4 != 0) {
    printf("Wrong width, must be multiple of 4 pixels\n");
    return false;
  }

  unsigned int r_pitch = (width * 10) / 8;
  unsigned int r_height = height;

  printf("%-16s%-32d\n%-16s%-32d\n", "Pitch:", r_pitch, "Height:", r_height);
  output_buf_size = r_pitch * r_height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  return true;
}

bool generate_raw12_output_buf() {
  if (width % 4 != 0) {
    printf("Wrong width, must be multiple of 4 pixels\n");
    return false;
  }

  unsigned int r_pitch = (width * 12) / 8;
  unsigned int r_height = height;

  printf("%-16s%-32d\n%-16s%-32d\n", "Pitch:", r_pitch, "Height:", r_height);
  output_buf_size = r_pitch * r_height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  return true;
}

bool generate_yuv422_output_buf() {
  unsigned int y_pitch = width;
  unsigned int y_height = height;
  unsigned int c_pitch = y_pitch;
  unsigned int c_height = y_height;

  printf("%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n", "Pitch-y:",
         y_pitch, "Height-y:", y_height, "Pitch-c:", c_pitch, "Height-c:",
         c_height);
  output_buf_size = y_pitch * y_height + c_pitch * c_height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < y_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += y_pitch;
    read_height++;
  }

  read_height = 0;
  while (read_height < c_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += c_pitch;
    read_height++;
  }
  return true;
}

bool generate_yuv422sp_output_buf() {
  unsigned int y_pitch = width;
  unsigned int y_height = height;
  unsigned int c_pitch = y_pitch;
  unsigned int c_height = y_height;

  printf("%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n", "Pitch-y:",
         y_pitch, "Height-y:", y_height, "Pitch-c:", c_pitch, "Height-c:",
         c_height);
  output_buf_size = y_pitch * y_height + c_pitch * c_height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < y_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += y_pitch;
    read_height++;
  }

  unsigned int read_pix = 0;
  while (read_pix < c_height * c_pitch / 2) {
    long u_pix_read = fread(p_output_buf, 1, 1, input_fd);
    if (u_pix_read != 1) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += 2;
    read_pix++;
  }

  fseek(input_fd, y_pitch * y_height + c_height * c_pitch / 2, SEEK_SET);
  p_output_buf = output_buf + y_pitch * y_height + 1;
  read_pix = 0;
  while (read_pix < c_height * c_pitch / 2) {
    long v_pix_read = fread(p_output_buf, 1, 1, input_fd);
    if (v_pix_read != 1) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += 2;
    read_pix++;
  }
  return true;
}

bool generate_yuv444888_output_buf() {
  unsigned int y_pitch = width;
  unsigned int y_height = height;
  unsigned int c_pitch = y_pitch;
  unsigned int c_height = y_height;

  printf("%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n%-16s%-32d\n", "Pitch-y:",
         y_pitch, "Height-y:", y_height, "Pitch-c:", c_pitch, "Height-c:",
         c_height);
  output_buf_size = y_pitch * y_height + c_pitch * c_height * 2;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < y_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += y_pitch;
    read_height++;
  }

  read_height = 0;
  while (read_height < c_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += c_pitch;
    read_height++;
  }

  read_height = 0;
  while (read_height < c_height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += c_pitch;
    read_height++;
  }
  return true;
}

bool generate_yv12_output_buf() {
  output_buf_size = width * height + width * height/2;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += width;
    read_height++;
  }

  read_height = 0;
  while (read_height < height) {
    long line_read = fread(p_output_buf, 1, width / 2, input_fd);
    if (line_read != width / 2) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += width / 2;
    read_height++;
  }
  return true;
}

bool generate_y8_output_buf() {
  output_buf_size = width * height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < height) {
    long line_read = fread(p_output_buf, 1, width, input_fd);
    if (line_read != width) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += width;
    read_height++;
  }

  return true;
}

bool generate_y16_output_buf() {
  output_buf_size = width * 2 * height;
  output_buf = (char *)malloc(output_buf_size);
  memset(output_buf, 0, output_buf_size);

  fseek(input_fd, 0, SEEK_SET);
  char *p_output_buf = output_buf;
  unsigned int read_height = 0;
  while (read_height < height) {
    long line_read = fread(p_output_buf, 1, width * 2, input_fd);
    if (line_read != width * 2) {
      printf("Failed to read resource file\n");
      return false;
    }
    p_output_buf += width * 2;
    read_height++;
  }

  return true;
}

int main(int argc, char *argv[]) {
  printf("\n\n");
  parse_args(argc, argv);
  unsigned int valid_input_output_format = 0;
  for (int i_output = 0; i_output < OUTPUT_FORMAT_NUM; i_output++) {
    if (!strcmp(output_input_mapping[i_output].output_format, output_format)) {
      for (int i_input = 0; i_input < INPUT_FORMAT_NUM; i_input++) {
        if (!strcmp(output_input_mapping[i_output].input_format_list[i_input],
                    input_format)) {
          valid_input_output_format = 1;
          break;
        } else if (!strcmp(output_input_mapping[i_output]
                               .input_format_list[i_input],
                           "*")) {
          valid_input_output_format = 2;
          break;
        }
      }
    };
  };

  if (valid_input_output_format == 0) {
    printf(
        "Invalid formats mapping of between input_format and output_format\n");
    exit(EXIT_FAILURE);
  }

  printf(
      "Image Width: \t%d\nImage Height: \t%d\nInput Format:\t%s\nOutput "
      "Format:\t%s\n",
      width, height, input_format, output_format);

  if (valid_input_output_format == 1) {
    input_fd = fopen(input_raw, "r");
    if (!input_fd) {
      printf("Could not open the resource file");
      exit(EXIT_FAILURE);
    }
  }

  bool ret = true;
  if (!strcmp(output_format, "yv12")) {
    ret = generate_yv12_output_buf();
  } else if (!strcmp(output_format, "y8")) {
    ret = generate_y8_output_buf();
  } else if (!strcmp(output_format, "y16")) {
    ret = generate_y16_output_buf();
  } else if (!strcmp(output_format, "ycbcr_420_888")) {
    printf("please use nv12 raw file: %s directly\n", input_raw);
  } else if (!strcmp(output_format, "ycrcb_420_sp")){
    printf("please use nv21 raw file: %s directly\n", input_raw);
  } else if (!strcmp(output_format, "nv12_linear_cam_intel")) {
    printf("please use nv12 raw file: %s directly\n", input_raw);
  } else if (!strcmp(output_format, "ycbcr_422_888")) {
    ret = generate_yuv422_output_buf();
  } else if (!strcmp(output_format, "ycbcr_422_sp")) {
    ret = generate_yuv422sp_output_buf();
  } else if (!strcmp(output_format, "ycbcr_444_888")) {
    ret = generate_yuv444888_output_buf();
  } else if (!strcmp(output_format, "nv12_y_tiled_intel")) {
    printf("please use nv12 raw file: %s directly\n", input_raw);
  } else if (!strcmp(output_format, "ycbcr_422_i")) {
    printf("please use yuyv422 raw file: %s directly\n", input_raw);
  } else if (!strcmp(output_format, "raw10")) {
    ret = generate_raw10_output_buf();
  } else if (!strcmp(output_format, "raw12")) {
    ret = generate_raw12_output_buf();
  } else if (!strcmp(output_format, "raw16")) {
    ret = generate_raw16_output_buf();
  } else if (!strcmp(output_format, "rawblob")) {
    ret = generate_rawblob_output_buf();
  } else if (!strcmp(output_format, "rawopaque")) {
    ret = generate_rawopaque_output_buf();
  }

  if (!ret) {
    if (!output_buf)
      free(output_buf);
    printf("Failed to generate %s output buf", output_format);
    if (input_fd)
      fclose(input_fd);
    fclose(output_fd);
    exit(EXIT_FAILURE);
  }

  if (output_buf) {
    output_fd = fopen(output_raw, "w+");
    if (!output_fd) {
      printf("Could not create the test img file");
      free(output_buf);
      if (input_fd)
        fclose(input_fd);
      exit(EXIT_FAILURE);
    }

    printf("Total output size: %ld\n", output_buf_size);
    long write_size = fwrite(output_buf, 1, output_buf_size, output_fd);
    if (write_size != output_buf_size) {
      printf("Failed to write output file\n");
      free(output_buf);
      if (input_fd)
        fclose(input_fd);
      fclose(output_fd);
      exit(EXIT_FAILURE);
    }
    printf("Generated test image file: %s\n", output_raw);
    free(output_buf);
  }

  if (input_fd)
    fclose(input_fd);
  if (output_fd)
    fclose(output_fd);

  exit(0);
}
