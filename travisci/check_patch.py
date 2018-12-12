#!/usr/bin/env python3

from subprocess import Popen, PIPE, STDOUT

# Run clang to check if code changes cause a diff output and return 1 if so.
cmd = "git show origin/master..@ | clang-format-diff-3.9 -p 1 -style=file"
diff = Popen(cmd, stdout=PIPE, shell=True).communicate()[0]
if diff:
    print("Code formatting is not according to style guidelines. Read:\n"
          "https://github.com/intel/IA-Hardware-Composer/wiki/Contributions#conformance-and-documentation-requirements")
    exit(1)

# Run cppcheck, on fail return msg and exit code 1.
cmd = 'cppcheck --quiet --force --error-exitcode=1 .'
out = Popen(cmd, stderr=STDOUT, stdout=PIPE, shell=True)
msg = out.communicate()[0]
if out.returncode:
    print(msg)
    exit(1)

exit(0)
