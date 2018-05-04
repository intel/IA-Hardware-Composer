import subprocess

# Run clang to check if code changes cause a diff output.
cmd = "git show origin/master..@ | clang-format-diff-3.9 -p 1 -style=file"
(diff, err) = subprocess.Popen(cmd, stdout=subprocess.PIPE,
shell=True).communicate()
if diff:
    print("Code formatting is not according to style guidelines. Read https://github.com/intel/IA-Hardware-Composer/wiki/Contributions#coding_style")
    exit(1)
exit(0)
