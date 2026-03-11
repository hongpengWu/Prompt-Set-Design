# Copyright (C) 2024
set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 0
set VIVADO_IMPL 0

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]

# OpenCV settings
set opencv_include "/usr/include/opencv4"
set opencv_lib "/lib/x86_64-linux-gnu"
set opencv_libs "-lopencv_imgcodecs -lopencv_imgproc -lopencv_core"
set stdcxx_lib "/lib/x86_64-linux-gnu"

# Project settings
set PROJ "hs_v6_prj"
set SOLN "sol1"
set XPART xc7z020-clg484-1
set CLKP 10.0

open_project -reset $PROJ

add_files "$SCRIPT_DIR/hs_accel.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"

# TB Flags
set TB_CFLAGS "-I$SCRIPT_DIR -I$opencv_include -std=c++14"
set TB_LDFLAGS "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"

add_files -tb "$SCRIPT_DIR/hs_tb.cpp" -cflags "$TB_CFLAGS" -csimflags "$TB_LDFLAGS"

set_top hls_HS

open_solution -reset $SOLN

set_part $XPART
create_clock -period $CLKP
set_clock_uncertainty 10%

set IMG0 "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png"
set IMG1 "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png"

if {$CSIM == 1} {
  puts "Running CSIM..."
  csim_design -ldflags "$TB_LDFLAGS" -argv "$IMG0 $IMG1"
}

if {$CSYNTH == 1} {
  puts "Running CSYNTH..."
  csynth_design
}

if {$COSIM == 1} {
  puts "Running COSIM..."
  cosim_design -ldflags "$TB_LDFLAGS" -argv "$IMG0 $IMG1"
}

exit
