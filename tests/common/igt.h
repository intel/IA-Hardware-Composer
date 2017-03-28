/*
* Copyright:
* code of this file copied/modified from
* https://anongit.freedesktop.org/git/xorg/app/intel-gpu-tools.git
* commit: 0a0eb5d48bab8df3e2fd530fb631e1478c9c9215
*/

#ifndef IGT_H
#define IGT_H

#define MAX_CRC_ENTRIES 10
#define MAX_LINE_LEN (10 + 11 * MAX_CRC_ENTRIES + 1)
#define LEGACY_LINE_LEN (6 * 8 + 5 + 1)
#define DRM_MAX_CRC_NR 10

typedef struct {
  uint32_t frame;
  bool has_valid_frame;
  int n_words;
  uint32_t crc[DRM_MAX_CRC_NR];
} igt_crc_t;

struct _igt_pipe_crc {
  int ctl_fd;
  int crc_fd;
  int flags;
  bool is_legacy;
  int pipe;
  int source;
};

typedef struct _igt_pipe_crc igt_pipe_crc_t;

typedef struct {
  char root[128];
  char dri_path[128];
} igt_debugfs_t;

const char *igt_debugfs_mount(void);
static bool __igt_debugfs_init(igt_debugfs_t *debugfs);
static igt_debugfs_t *__igt_debugfs_singleton(void);
FILE *igt_debugfs_fopen(const char *filename, const char *mode);
int igt_debugfs_open(const char *filename, int mode);
const char *kmstest_pipe_name(int pipe);
igt_pipe_crc_t *pipe_crc_new(int pipe);
bool igt_pipe_crc_stop(igt_pipe_crc_t *pipe_crc);
bool igt_pipe_crc_collect_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out_crc);
bool igt_assert_crc_equal(const igt_crc_t *a, const igt_crc_t *b);

#endif
