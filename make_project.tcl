# ==============================================================================
# Vivado tcl script for building RedPitaya FPGA project.
# Modified by A. Trost 2019, M. Adamic 2020.
#
# To create a demo 2-channel TDC system,
# run this script from the base folder inside Vivado tcl console.
# ==============================================================================

# List board files

set_param board.repoPaths [list board]

# Set project properties

set project_name TDCsystem
set project_part xc7z010clg400-1
set project_target_language VHDL

# Create project for Red Pitaya

create_project $project_name ./$project_name -part $project_part
set_property BOARD_PART redpitaya.com:redpitaya:part0:1.1 [current_project]
set_property target_language $project_target_language [current_project]

# Set IP repository path

set_property  ip_repo_paths  ./AXITDC [current_project]
update_ip_catalog

# Add sources (constraints)

add_files -fileset constrs_1  ./src/ports.xdc
add_files -fileset constrs_1  ./src/timing.xdc

# Create and validate Block Design from exported .tcl file

source ./src/TDCsystem_bd.tcl
validate_bd_design

# Create HDL wrapper and add it to the fileset

make_wrapper -files [get_files ./$project_name/$project_name.srcs/sources_1/bd/$project_name/$project_name.bd] -top
add_files -norecurse ./$project_name/$project_name.srcs/sources_1/bd/$project_name/hdl/${project_name}_wrapper.vhd

# Generate output products

#generate_target all [get_files ./$project_name/$project_name.srcs/sources_1/bd/$project_name/$project_name.bd]

