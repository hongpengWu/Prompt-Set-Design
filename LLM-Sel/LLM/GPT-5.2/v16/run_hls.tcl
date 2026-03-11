open_project -reset hls_hs_prj

add_files ./hs_accel.cpp -cflags "-I. -std=c++14"
add_files -tb ./hs_tb.cpp -cflags "-I. -std=c++14"

set_top hls_HS

open_solution -reset sol1
set_part {xc7z020-clg484-1}
create_clock -period 10.0 -name default
set_clock_uncertainty 10%

set_param general.maxThreads 25

set IMG0 "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png"
set IMG1 "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png"
set LDFLAGS "-lpng16 -lz"

csim_design -ldflags $LDFLAGS -argv "$IMG0 $IMG1"
csynth_design
cosim_design -ldflags $LDFLAGS -argv "$IMG0 $IMG1"

exit
