#!/system/bin/sh
/vendor/bin/hwcservice_test -m 0
screen_res=`/vendor/bin/hwcservice_test -g | sed -n "/^[0-9]/p" | sed -n "s/\(^[0-9]*\)[ \t]*\([0-9]*\)[ \t]*.*/\1x\2/p"`
echo "screen_res: $screen_res"
wm size $screen_res
