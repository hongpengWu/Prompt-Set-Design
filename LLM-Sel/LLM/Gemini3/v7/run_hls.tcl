# Copyright (C) 2024
set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 0
set VIVADO_IMPL 0

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]

# OpenCV Configuration (from reference)
set opencv_include "/usr/include/opencv4"
set opencv_lib "/lib/x86_64-linux-gnu"
set opencv_libs "-lopencv_imgcodecs -lopencv_imgproc -lopencv_core"
set stdcxx_lib "/lib/x86_64-linux-gnu"

set ::env(CC) "/usr/bin/gcc"
set ::env(CXX) "/usr/bin/g++"
if {[info exists ::env(LD_LIBRARY_PATH)]} {
  set ::env(LD_LIBRARY_PATH) "$stdcxx_lib:$opencv_lib:$::env(LD_LIBRARY_PATH)"
} else {
  set ::env(LD_LIBRARY_PATH) "$stdcxx_lib:$opencv_lib"
}
set ::env(LD_PRELOAD) "/lib/x86_64-linux-gnu/libstdc++.so.6"

set PROJ "hls_hs_prj"
set SOLN "sol1"
set XPART xc7z020-clg484-1
set CLKP 10.0

open_project -reset $PROJ

# Add design files
add_files "$SCRIPT_DIR/hs_accel.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"

# Add testbench files
add_files -tb "$SCRIPT_DIR/hs_tb.cpp" -cflags "-I$SCRIPT_DIR -I$opencv_include -std=c++14" -csimflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"

set_top hls_HS

open_solution -reset $SOLN

set_part $XPART
create_clock -period $CLKP
set_clock_uncertainty 10%

# Compiler and Linker flags for OpenCV
set LD_FLAGS "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
set IMG0 "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png"
set IMG1 "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png"

if {$CSIM == 1} {
  csim_design -ldflags "${LD_FLAGS}" -argv "$IMG0 $IMG1"
}

if {$CSYNTH == 1} {
  csynth_design
}

if {$COSIM == 1} {
  cosim_design -ldflags "${LD_FLAGS}" -argv "$IMG0 $IMG1"
}

exit
