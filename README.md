# fpga_zynq_board_7000
How It Works

Camera feeds frames to the ARM processor
Frames are sent to FPGA logic through AXI
FPGA performs parallel image processing
Processed output is returned to the ARM
ARM-side app displays or logs the results in real time

🏗️ Build & Run Instructions
1. Build Hardware in Vivado
Import block design
Configure Zynq PS
Add custom AXI peripheral
Generate bitstream
Export .xsa

2. Build Linux Image with PetaLinux
petalinux-create -t project --template zynq --name fpga_image_project
petalinux-config --get-hw-description=path/to/xsa/
petalinux-build

3. Deploy
Flash SD card
Boot PetaLinux
Load bitstream
Run ARM-side C program
