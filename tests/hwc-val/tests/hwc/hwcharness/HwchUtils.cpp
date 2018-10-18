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

#include <dirent.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Check to see if a process is running on the Android system
// Pass in the binary name is a pattern e.g. "surfaceflinger"
bool processRunning(char *pattern) {
  regex_t number;
  regex_t name;
  regcomp(&number, "^[0-9]+$", REG_EXTENDED);
  regcomp(&name, pattern, 0);

  if (chdir("/proc") == 0) {
    DIR *proc = opendir("/proc");

    // Look for all the directories in /proc
    struct dirent *dp;
    while ((dp = readdir(proc))) {
      // Match those which are numerical
      if ((regexec(&number, dp->d_name, 0, 0, 0)) == 0) {
        chdir(dp->d_name);
        char buf[4096];
        int fd = open("cmdline", O_RDONLY);
        buf[read(fd, buf, (sizeof buf) - 1)] = '\0';
        if (regexec(&name, buf, 0, 0, 0) == 0) {
          return true;
        }
        close(fd);
        chdir("..");
      }
    }
    closedir(proc);
  }

  return false;
}
