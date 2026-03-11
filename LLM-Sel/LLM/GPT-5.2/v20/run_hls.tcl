set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 0
set VIVADO_IMPL 0

set max_threads 24

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]

set ::env(CC) "/usr/bin/gcc"
set ::env(CXX) "/usr/bin/g++"

set PROJ "hls_hs_prj"
set SOLN "sol1"
set XPART xc7z020-clg484-1
set CLKP 10.0

open_project -reset $PROJ

add_files "$SCRIPT_DIR/hs_accel.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"
add_files -tb "$SCRIPT_DIR/hs_tb.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"

set_top hls_HS

open_solution -reset $SOLN
set_part $XPART
create_clock -period $CLKP
set_clock_uncertainty 10%

set LD_FLAGS "-lpng -lz"
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
