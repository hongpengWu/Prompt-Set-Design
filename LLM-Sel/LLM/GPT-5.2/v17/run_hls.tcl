set_param general.maxThreads 25

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]

set opencv_include "/usr/include/opencv4"
set opencv_lib "/lib/x86_64-linux-gnu"
set opencv_libs "-lopencv_imgcodecs -lopencv_imgproc -lopencv_core"
set stdcxx_lib "/lib/x86_64-linux-gnu"

if {[info exists ::env(LD_LIBRARY_PATH)]} {
  set ::env(LD_LIBRARY_PATH) "$stdcxx_lib:$opencv_lib:$::env(LD_LIBRARY_PATH)"
} else {
  set ::env(LD_LIBRARY_PATH) "$stdcxx_lib:$opencv_lib"
}
set ::env(LD_PRELOAD) "/lib/x86_64-linux-gnu/libstdc++.so.6"

open_project -reset hls_hs_prj

add_files "$SCRIPT_DIR/hs_accel.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"
add_files -tb "$SCRIPT_DIR/hs_tb.cpp" -cflags "-I$SCRIPT_DIR -I$opencv_include -std=c++14" -csimflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"

set_top hls_HS

open_solution -reset sol1
set_part {xc7z020-clg484-1}
create_clock -period 10.0 -name default
set_clock_uncertainty 10%

set LDFLAGS "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"

set IMG0 /home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png
set IMG1 /home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png
set ARGS "$IMG0 $IMG1"

csim_design -ldflags $LDFLAGS -argv $ARGS
csynth_design
cosim_design -ldflags $LDFLAGS -argv $ARGS

exit
