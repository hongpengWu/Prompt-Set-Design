# Copyright (C) 2024
set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 0
set VIVADO_IMPL 0

set max_threads 24

set CUR_DIR [pwd]
if {[info exists ::env(OPENCV_INCLUDE)]} {
  set OPENCV_INCLUDE $::env(OPENCV_INCLUDE)
} else {
  set OPENCV_INCLUDE "/usr/include/opencv4"
}
if {[info exists ::env(OPENCV_LIB)]} {
  set OPENCV_LIB $::env(OPENCV_LIB)
} else {
  set OPENCV_LIB "/usr/lib/x86_64-linux-gnu"
}
set OPENCV_LIBS "-lopencv_imgcodecs -lopencv_imgproc -lopencv_core -lopencv_highgui -lopencv_flann -lopencv_features2d"
set STDCXX_LIB "/usr/lib/x86_64-linux-gnu"

set PROJ "hls_hs_prj"
set SOLN "sol1"
set XPART xc7z020-clg484-1
set CLKP 10.0

open_project -reset $PROJ

# Add design files
add_files "hs_accel.cpp" -cflags "-I${CUR_DIR} -std=c++14"

# Add testbench files
add_files -tb "hs_tb.cpp" -cflags "-I${CUR_DIR} -I${OPENCV_INCLUDE} -std=c++14" -csimflags "-I${CUR_DIR} -std=c++14"

set_top hls_HS

open_solution -reset $SOLN

set_part $XPART
create_clock -period $CLKP
set_clock_uncertainty 10%

# Compiler and Linker flags for OpenCV
set LD_FLAGS "-L${STDCXX_LIB} -Wl,-rpath,${STDCXX_LIB} -L${OPENCV_LIB} -Wl,-rpath,${OPENCV_LIB} -Wl,--allow-shlib-undefined ${OPENCV_LIBS}"

if {$CSIM == 1} {
  csim_design -ldflags "${LD_FLAGS}" -argv "/root/autodl-tmp/whp/Experiment/Math_baseline/yos9.tif /root/autodl-tmp/whp/Experiment/Math_baseline/yos10.tif"
}

if {$CSYNTH == 1} {
  csynth_design
}

if {$COSIM == 1} {
  cosim_design -ldflags "${LD_FLAGS}" -argv "/root/autodl-tmp/whp/Experiment/Math_baseline/yos9.tif /root/autodl-tmp/whp/Experiment/Math_baseline/yos10.tif"
}

# Summarize Estimated Clock and Cosim Latency to compute T_exec
set csynth_xml "$PROJ/$SOLN/syn/report/hls_HS_csynth.xml"
set cosim_rpt "$PROJ/$SOLN/sim/report/hls_HS_cosim.rpt"

set est_clk ""
set lat_min ""
set lat_avg ""
set lat_max ""
set total_cycles ""

if {[file exists $csynth_xml]} {
  set fp [open $csynth_xml r]
  set data [read $fp]
  close $fp
  if {[regexp -line {<EstimatedClockPeriod>([0-9.]+)</EstimatedClockPeriod>} $data -> est]} {
    set est_clk $est
  }
}

if {[file exists $cosim_rpt]} {
  set fp [open $cosim_rpt r]
  set rpt [read $fp]
  close $fp
  if {[regexp -line {\|\s*Verilog\|\s*Pass\|\s*([0-9]+)\|\s*([0-9]+)\|\s*([0-9]+)\|.*\|\s*([0-9]+)\|} $rpt -> lat_min lat_avg lat_max total_cycles]} {
  }
}

set t_exec ""
if {![string equal $est_clk ""] && ![string equal $total_cycles ""]} {
  set t_exec [format "%.3f" [expr {$est_clk * $total_cycles}]]
}

file mkdir reports
set summary_file "reports/summary_T_exec.txt"
set sfp [open $summary_file w]
puts $sfp "EstimatedClockPeriod = $est_clk ns"
puts $sfp "TotalExecution(cycles) = $total_cycles cycles"
puts $sfp "T_exec = EstimatedClockPeriod × TotalExecution(cycles) = $t_exec ns"
close $sfp

set sol_report_dir "$PROJ/$SOLN/syn/report"
file mkdir $sol_report_dir
set summary_file2 "$sol_report_dir/summary_T_exec.txt"
set sfp2 [open $summary_file2 w]
puts $sfp2 "EstimatedClockPeriod = $est_clk ns"
puts $sfp2 "TotalExecution(cycles) = $total_cycles cycles"
puts $sfp2 "T_exec = EstimatedClockPeriod × TotalExecution(cycles) = $t_exec ns"
close $sfp2

exit
