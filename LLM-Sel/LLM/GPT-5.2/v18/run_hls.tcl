open_project -reset hs_prj

set_top hls_HS

add_files hs_accel.cpp
add_files -tb hs_tb.cpp

open_solution -reset sol1 -flow_target vivado

set_part xc7z020clg400-1
create_clock -period 10 -name default

if {[info exists ::env(CSIM)]} { set CSIM $::env(CSIM) } else { set CSIM 1 }
if {[info exists ::env(CSYNTH)]} { set CSYNTH $::env(CSYNTH) } else { set CSYNTH 1 }
if {[info exists ::env(COSIM)]} { set COSIM $::env(COSIM) } else { set COSIM 1 }

set LD_FLAGS "-lpng -lz"

if {$CSIM == 1} {
  csim_design -clean -O -ldflags "${LD_FLAGS}"
}

if {$CSYNTH == 1} {
  csynth_design
}

if {$COSIM == 1} {
  cosim_design -ldflags "${LD_FLAGS}"
}

exit
