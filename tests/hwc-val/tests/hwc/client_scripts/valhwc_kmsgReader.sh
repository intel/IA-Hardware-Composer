#!/system/bin/sh
echo "RUNNING kmsgReader $*"

# Argument processing
FTRACE=0
for i in $@
do
    if [ $i == "-ftrace" ]
    then
        FTRACE=1
    fi
done

if [ $FTRACE -eq 1 ]
then
    # Set buffer size for ftrace
    cd /sys/kernel/debug/tracing
    echo 4096 > buffer_size_kb

    # Function trace
    echo function_graph > current_tracer
    echo i915_* drm_* intel_* vlv_* valleyview_* skl_* skylake_* haswell_* > set_ftrace_filter

    # MMIO trace
    echo 1 > /sys/kernel/debug/tracing/events/i915/i915_reg_rw/enable

    # Clear old trace
    echo 0 > tracing_on
    echo 0 > trace
    echo 1 > tracing_on
else
    # Disable trace
    cd /sys/kernel/debug/tracing
    echo 0 > tracing_on
fi

#valhwc_kmsgReader $*
