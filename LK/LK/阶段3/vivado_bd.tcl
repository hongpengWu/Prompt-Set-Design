set script_dir [file dirname [file normalize [info script]]]
set proj_name "m2hls_bd"
set proj_dir [file join $script_dir "vivado_proj"]

set part "xc7z020clg400-1"

set hls_ip_repo [file normalize [file join $script_dir ".." "hls_hs_prj" "sol1" "impl" "ip"]]

file mkdir $proj_dir
create_project $proj_name $proj_dir -part $part -force

if {[file exists $hls_ip_repo]} {
  set_property ip_repo_paths [list $hls_ip_repo] [current_project]
  update_ip_catalog
} else {
  puts "ERROR: HLS IP repo not found: $hls_ip_repo"
  exit 1
}

create_bd_design "design_1"

set ps7 [create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 ps7]
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "true"} $ps7
set_property -dict [list CONFIG.PCW_USE_M_AXI_GP0 {1} CONFIG.PCW_USE_S_AXI_HP0 {1}] $ps7

set rst0 [create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst0]
connect_bd_net [get_bd_pins rst0/slowest_sync_clk] [get_bd_pins ps7/FCLK_CLK0]
connect_bd_net [get_bd_pins rst0/ext_reset_in] [get_bd_pins ps7/FCLK_RESET0_N]

connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins ps7/M_AXI_GP0_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins ps7/S_AXI_HP0_ACLK]
if {[llength [get_bd_pins -quiet ps7/S_AXI_HP0_ARESETN]]} {
  connect_bd_net [get_bd_pins rst0/peripheral_aresetn] [get_bd_pins ps7/S_AXI_HP0_ARESETN]
}

set axi_ctrl [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ctrl]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] $axi_ctrl

set axi_mem_ic [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_mem_ic]
set_property -dict [list CONFIG.NUM_SI {4} CONFIG.NUM_MI {1}] $axi_mem_ic

set axi_dw_hp0 [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dwidth_converter:2.1 axi_dw_hp0]
set axi_pc_hp0 [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_protocol_converter:2.1 axi_pc_hp0]

set hs0 {}
if {[llength [get_ipdefs xilinx.com:hls:hls_HS:1.0]]} {
  set hs0 [create_bd_cell -type ip -vlnv xilinx.com:hls:hls_HS:1.0 hs0]
} else {
  puts "ERROR: hls_HS HLS IP not found in repo $hls_ip_repo"
  puts "INFO: Available HLS IPs:"
  puts [get_ipdefs xilinx.com:hls:*]
  exit 1
}

connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins hs0/ap_clk]
if {[llength [get_bd_pins -quiet hs0/ap_rst_n]]} {
  connect_bd_net [get_bd_pins rst0/peripheral_aresetn] [get_bd_pins hs0/ap_rst_n]
} elseif {[llength [get_bd_pins -quiet hs0/ap_rst]]} {
  connect_bd_net [get_bd_pins rst0/peripheral_reset] [get_bd_pins hs0/ap_rst]
}

connect_bd_intf_net [get_bd_intf_pins ps7/M_AXI_GP0] [get_bd_intf_pins axi_ctrl/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_ctrl/M00_AXI] [get_bd_intf_pins hs0/s_axi_control]

connect_bd_intf_net [get_bd_intf_pins hs0/m_axi_gmem0] [get_bd_intf_pins axi_mem_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins hs0/m_axi_gmem1] [get_bd_intf_pins axi_mem_ic/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins hs0/m_axi_gmem2] [get_bd_intf_pins axi_mem_ic/S02_AXI]
connect_bd_intf_net [get_bd_intf_pins hs0/m_axi_gmem3] [get_bd_intf_pins axi_mem_ic/S03_AXI]

connect_bd_intf_net [get_bd_intf_pins axi_mem_ic/M00_AXI] [get_bd_intf_pins axi_dw_hp0/S_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_dw_hp0/M_AXI] [get_bd_intf_pins axi_pc_hp0/S_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_pc_hp0/M_AXI] [get_bd_intf_pins ps7/S_AXI_HP0]

connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_ctrl/ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_ctrl/S00_ACLK]
connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_ctrl/M00_ACLK]
connect_bd_net [get_bd_pins rst0/interconnect_aresetn] [get_bd_pins axi_ctrl/ARESETN]
connect_bd_net [get_bd_pins rst0/peripheral_aresetn] [get_bd_pins axi_ctrl/S00_ARESETN]
connect_bd_net [get_bd_pins rst0/peripheral_aresetn] [get_bd_pins axi_ctrl/M00_ARESETN]

if {[llength [get_bd_pins -quiet axi_mem_ic/ACLK]]} {
  connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_mem_ic/ACLK]
}
foreach pin_name {S00_ACLK S01_ACLK S02_ACLK S03_ACLK M00_ACLK} {
  set p [get_bd_pins -quiet "axi_mem_ic/$pin_name"]
  if {[llength $p]} {
    connect_bd_net [get_bd_pins ps7/FCLK_CLK0] $p
  }
}
if {[llength [get_bd_pins -quiet axi_mem_ic/ARESETN]]} {
  connect_bd_net [get_bd_pins rst0/interconnect_aresetn] [get_bd_pins axi_mem_ic/ARESETN]
}
foreach pin_name {S00_ARESETN S01_ARESETN S02_ARESETN S03_ARESETN M00_ARESETN} {
  set p [get_bd_pins -quiet "axi_mem_ic/$pin_name"]
  if {[llength $p]} {
    connect_bd_net [get_bd_pins rst0/peripheral_aresetn] $p
  }
}
foreach pin_name {aclk s_axi_aclk m_axi_aclk} {
  set p [get_bd_pins -quiet "axi_dw_hp0/$pin_name"]
  if {[llength $p]} {
    connect_bd_net [get_bd_pins ps7/FCLK_CLK0] $p
  }
}
foreach pin_name {aresetn s_axi_aresetn m_axi_aresetn} {
  set p [get_bd_pins -quiet "axi_dw_hp0/$pin_name"]
  if {[llength $p]} {
    connect_bd_net [get_bd_pins rst0/peripheral_aresetn] $p
  }
}
if {[llength [get_bd_pins -quiet axi_pc_hp0/aclk]]} {
  connect_bd_net [get_bd_pins ps7/FCLK_CLK0] [get_bd_pins axi_pc_hp0/aclk]
}
if {[llength [get_bd_pins -quiet axi_pc_hp0/aresetn]]} {
  connect_bd_net [get_bd_pins rst0/peripheral_aresetn] [get_bd_pins axi_pc_hp0/aresetn]
}

assign_bd_address

validate_bd_design
save_bd_design

generate_target all [get_files "$proj_dir/$proj_name.srcs/sources_1/bd/design_1/design_1.bd"]
make_wrapper -files [get_files "$proj_dir/$proj_name.srcs/sources_1/bd/design_1/design_1.bd"] -top
add_files -norecurse "$proj_dir/$proj_name.gen/sources_1/bd/design_1/hdl/design_1_wrapper.v"
update_compile_order -fileset sources_1

launch_runs synth_1 -jobs 4
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

set impl_dir [file join $proj_dir "$proj_name.runs" "impl_1"]
set bit_path [file join $impl_dir "design_1_wrapper.bit"]
if {[file exists $bit_path]} {
  file copy -force $bit_path [file join $script_dir "design_1.bit"]
} else {
  puts "WARNING: Bitstream not found at $bit_path"
}

write_hw_platform -fixed -include_bit -force [file join $script_dir "design_1.xsa"]
write_hwdef -force -file [file join $script_dir "design_1.hdf"]
if {[file exists [file join $script_dir "design_1.hdf"]]} {
  file copy -force [file join $script_dir "design_1.hdf"] [file join $script_dir "design_1.hwh"]
}

exit
