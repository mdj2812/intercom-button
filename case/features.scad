// Part 3: features — button hole, USB cutout, mic holes,
// PCB cradle, screw bosses, light pipe hole
include <params.scad>
include <shell.scad>

module button_hole() {
    // through top at (btn_x, btn_y), vertical
    translate([wall+btn_x, wall+btn_y, top_z(btn_x)-3])
        cylinder(h=10, d=btn_hole_d);
}

module usb_cutout() {
    // back wall, centered on PCB USB position
    translate([wall+in_l-1, wall+pcb_y0+pcb_w/2-6, wall+2])
        cube([wall+2, 12, 7]);
}

module mic_holes() {
    // 5 x 1.2mm sound holes over mic position
    for (dx=[-2:2])
        translate([wall+mic_x+dx*2.2, wall+mic_y, top_z(mic_x)-3])
            cylinder(h=10, d=1.2);
}

module lightpipe_hole() {
    // 3mm hole above DevKitC RGB LED (approx 15mm from PCB USB end)
    lx = pcb_x0 + pcb_l - 15;
    ly = pcb_y0 + pcb_w/2;
    translate([wall+lx, wall+ly, top_z(lx)-3])
        cylinder(h=10, d=3);
}

module cradle() {
    // rails holding DevKitC PCB edges; overlap 0.5 into back wall
    for (y=[pcb_y0-2, pcb_y0+pcb_w])
        translate([wall+pcb_x0, wall+y, wall])
            cube([in_l-pcb_x0+0.5, 2, 3]);
}

module bosses() {
    // 4 corner bosses (overlap 0.5 into both walls), M2 self-tapping
    for (x=[2.5, in_l-2.5], y=[2.5, in_w-2.5])
        translate([wall+x, wall+y, wall])
            difference() {
                cylinder(h=6, d=6);
                cylinder(h=6.1, d=1.6);
            }
}
