/*
* Copyright:
* code of this file copied/modified from
* https://anongit.freedesktop.org/git/xorg/app/intel-gpu-tools.git
* commit: 0a0eb5d48bab8df3e2fd530fb631e1478c9c9215
*/

#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <igt.h>

const char *kmstest_pipe_name(int pipe) {
  const char *str[] = {"A", "B", "C"};

  if (pipe == -1)
    return "None";

  if (pipe > 2)
    return "invalid";

  return str[pipe];
}

const char *igt_debugfs_mount(void) {
  struct stat st;

  if (stat("/debug/dri", &st) == 0)
    return "/debug";

  if (stat("/sys/kernel/debug/dri", &st) == 0)
    return "/sys/kernel/debug";

  return "/sys/kernel/debug";
}

static bool __igt_debugfs_init(igt_debugfs_t *debugfs) {
  struct stat st;
  int n;

  strcpy(debugfs->root, igt_debugfs_mount());
  for (n = 0; n < 16; n++) {
    int len = sprintf(debugfs->dri_path, "%s/dri/%d", debugfs->root, n);
    sprintf(debugfs->dri_path + len, "/i915_error_state");
    if (stat(debugfs->dri_path, &st) == 0) {
      debugfs->dri_path[len] = '\0';
      return true;
    }
  }

  debugfs->dri_path[0] = '\0';
  return false;
}

static igt_debugfs_t *__igt_debugfs_singleton(void) {
  static igt_debugfs_t singleton;
  static bool init_done = false;

  if (init_done)
    return &singleton;

  if (__igt_debugfs_init(&singleton)) {
    init_done = true;
    return &singleton;
  } else {
    return NULL;
  }
}

FILE *igt_debugfs_fopen(const char *filename, const char *mode) {
  char buf[1024];

  igt_debugfs_t *debugfs = __igt_debugfs_singleton();

  if (!debugfs)
    return NULL;

  sprintf(buf, "%s/%s", debugfs->dri_path, filename);
  return fopen(buf, mode);
}

int igt_debugfs_open(const char *filename, int mode) {
  char buf[1024];
  igt_debugfs_t *debugfs = __igt_debugfs_singleton();

  if (!debugfs)
    return -1;

  sprintf(buf, "%s/%s", debugfs->dri_path, filename);
  return open(buf, mode);
}

igt_pipe_crc_t *pipe_crc_new(int pipe) {
  igt_pipe_crc_t *pipe_crc;
  char buf[128];

  pipe_crc = (igt_pipe_crc_t *)calloc(1, sizeof(struct _igt_pipe_crc));

  sprintf(buf, "crtc-%d/crc/control", pipe);
  pipe_crc->ctl_fd = igt_debugfs_open(buf, O_WRONLY);
  if (pipe_crc->ctl_fd == -1) {
    pipe_crc->ctl_fd = igt_debugfs_open("i915_display_crc_ctl", O_WRONLY);
    pipe_crc->is_legacy = true;
  }

  if (pipe_crc->is_legacy) {
    sprintf(buf, "i915_pipe_%s_crc", kmstest_pipe_name(pipe));
    pipe_crc->crc_fd = igt_debugfs_open(buf, O_RDONLY);
  } else {
    pipe_crc->crc_fd = -1;
  }

  pipe_crc->pipe = pipe;
  pipe_crc->source = 9;
  pipe_crc->flags = O_RDONLY;

  return pipe_crc;
}

static const char *pipe_crc_sources[] = {"none", "plane1", "plane2", "pf",
                                         "pipe", "TV",     "DP-B",   "DP-C",
                                         "DP-D", "auto"};

static const char *pipe_crc_source_name(int source) {
  return pipe_crc_sources[source];
}

static bool igt_pipe_crc_do_start(igt_pipe_crc_t *pipe_crc) {
  char buf[64];

  /* Stop first just to make sure we don't have lingering state left. */
  igt_pipe_crc_stop(pipe_crc);

  if (pipe_crc->is_legacy)
    sprintf(buf, "pipe %s %s", kmstest_pipe_name(pipe_crc->pipe),
            pipe_crc_source_name(pipe_crc->source));
  else
    sprintf(buf, "%s", pipe_crc_source_name(pipe_crc->source));

  if (write(pipe_crc->ctl_fd, buf, strlen(buf)) != strlen(buf)) {
    return false;
  }

  if (!pipe_crc->is_legacy) {
    sprintf(buf, "crtc-%d/crc/data", pipe_crc->pipe);
    errno = 0;
    pipe_crc->crc_fd = igt_debugfs_open(buf, pipe_crc->flags);
    if (pipe_crc->crc_fd == -1)
      return false;
  }

  return true;
}

static bool pipe_crc_init_from_string(igt_pipe_crc_t *pipe_crc, igt_crc_t *crc,
                                      const char *line) {
  int n, i;
  const char *buf;

  if (pipe_crc->is_legacy) {
    crc->has_valid_frame = true;
    crc->n_words = 5;
    n = sscanf(line, "%8u %8x %8x %8x %8x %8x", &crc->frame, &crc->crc[0],
               &crc->crc[1], &crc->crc[2], &crc->crc[3], &crc->crc[4]);
    return n == 6;
  }

  if (strncmp(line, "XXXXXXXXXX", 10) == 0)
    crc->has_valid_frame = false;
  else {
    crc->has_valid_frame = true;
    crc->frame = strtoul(line, NULL, 16);
  }

  buf = line + 10;
  for (i = 0; *buf != '\n'; i++, buf += 11)
    crc->crc[i] = strtoul(buf, NULL, 16);

  crc->n_words = i;

  return true;
}

static int read_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out) {
  ssize_t bytes_read;
  char buf[MAX_LINE_LEN + 1];
  size_t read_len;

  if (pipe_crc->is_legacy)
    read_len = LEGACY_LINE_LEN;
  else
    read_len = MAX_LINE_LEN;

  bytes_read = read(pipe_crc->crc_fd, &buf, read_len);

  if (bytes_read < 0)
    if (!(pipe_crc->flags & O_NONBLOCK))
      return -EINVAL;

  if (bytes_read < 0)
    bytes_read = 0;

  buf[bytes_read] = '\0';

  if (bytes_read && !pipe_crc_init_from_string(pipe_crc, out, buf))
    return -EINVAL;

  return bytes_read;
}

static void read_one_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out) {
  while (read_crc(pipe_crc, out) == 0)
    usleep(1000);
}

void igt_pipe_crc_start(igt_pipe_crc_t *pipe_crc) {
  igt_crc_t crc;

  igt_pipe_crc_do_start(pipe_crc);

  if (pipe_crc->is_legacy) {
    /*
     * For some no yet identified reason, the first CRC is
     * bonkers. So let's just wait for the next vblank and read
     * out the buggy result.
     *
     * On CHV sometimes the second CRC is bonkers as well, so
     * don't trust that one either.
     */
    read_one_crc(pipe_crc, &crc);
    read_one_crc(pipe_crc, &crc);
  }
}

bool igt_pipe_crc_stop(igt_pipe_crc_t *pipe_crc) {
  char buf[32];

  if (pipe_crc->is_legacy) {
    sprintf(buf, "pipe %s none", kmstest_pipe_name(pipe_crc->pipe));
    if (write(pipe_crc->ctl_fd, buf, strlen(buf)) != strlen(buf))
      return true;
  } else {
    close(pipe_crc->crc_fd);
    pipe_crc->crc_fd = -1;
  }
  return false;
}

int igt_pipe_crc_get_crcs(igt_pipe_crc_t *pipe_crc, int n_crcs,
                          igt_crc_t **out_crcs) {
  igt_crc_t *crcs;
  int n = 0;

  crcs = (igt_crc_t *)calloc(n_crcs, sizeof(igt_crc_t));

  do {
    igt_crc_t *crc = &crcs[n];
    int ret;

    ret = read_crc(pipe_crc, crc);
    if (ret < 0)
      continue;
    if (ret == 0)
      break;

    n++;
  } while (n < n_crcs);

  *out_crcs = crcs;
  return n;
}

static void crc_sanity_checks(igt_crc_t *crc) {
  int i;
  bool all_zero = true;

  for (i = 0; i < crc->n_words; i++) {
    if (crc->crc[i])
      all_zero = false;
  }
}

bool igt_pipe_crc_collect_crc(igt_pipe_crc_t *pipe_crc, igt_crc_t *out_crc) {
  igt_pipe_crc_start(pipe_crc);
  read_one_crc(pipe_crc, out_crc);
  if (igt_pipe_crc_stop(pipe_crc))
    return true;

  crc_sanity_checks(out_crc);
  return false;
}

bool igt_assert_crc_equal(const igt_crc_t *a, const igt_crc_t *b) {
  for (int i = 0; i < a->n_words; i++) {
    if (a->crc[i] != b->crc[i]) {
      return false;
    }
  }
  return true;
}
