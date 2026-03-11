set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 0
set VIVADO_IMPL 0

set max_threads 8

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]
cd $SCRIPT_DIR

set PROJ "ukf2hls_prj"
set SOLN "sol1"
set XPART xc7z020-clg484-1
set CLKP 10.0
set TB_SEL 0

open_project -reset $PROJ

add_files "$SCRIPT_DIR/ukf_accel.cpp" -cflags "-I$SCRIPT_DIR -std=c++14"
set TB_FILE "$SCRIPT_DIR/ukf_tb.cpp"
if {$TB_SEL == 1} {
  set TB_FILE "$SCRIPT_DIR/ukf_tb_baseline.cpp"
}
add_files -tb "$TB_FILE" -cflags "-I$SCRIPT_DIR -std=c++14"

set_top hls_SR_UKF

open_solution -reset $SOLN

set_part $XPART
create_clock -period $CLKP
set_clock_uncertainty 10%

set LD_FLAGS ""

if {$CSIM == 1} {
  csim_design -ldflags "${LD_FLAGS}"
}

if {$CSYNTH == 1} {
  csynth_design
}

if {$COSIM == 1} {
  cosim_design -rtl verilog -ldflags "${LD_FLAGS}"
}

set csynth_xml "$PROJ/$SOLN/syn/report/hls_SR_UKF_csynth.xml"
if {![file exists $csynth_xml]} {
  puts "INFO: csynth report not found; running csynth_design for export_design"
  csynth_design
}

set export_args [list -format ip_catalog -rtl verilog]
set export_flow "none"
if {$VIVADO_IMPL == 1} {
  set export_flow "impl"
} elseif {$VIVADO_SYN == 1} {
  set export_flow "syn"
}
lappend export_args -flow $export_flow
puts "INFO: export_design $export_args"
eval export_design $export_args

set cosim_rpt "$PROJ/$SOLN/sim/report/hls_SR_UKF_cosim.rpt"

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
