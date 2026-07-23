// Part 4: assembly + bottom cover + render targets
include <params.scad>
include <shell.scad>
include <features.scad>

// ---- Main body ----
module body() {
    difference() {
        union() {
            shell();
            cradle();
            bosses();
        }
        button_hole();
        usb_cutout();
        mic_holes();
        lightpipe_hole();
        // clearance for button body below top
        translate([wall+btn_x, wall+btn_y, wall])
            cylinder(h=btn_body_len, d=btn_body_d);
    }
}

// ---- Bottom cover ----
module cover() {
    difference() {
        // flat plate, screws into bosses provide alignment
        translate([wall, wall, 0])
            hull() corner_posts(in_l, in_w, 1.6, 1.6,
                                max(corner_r-wall-0.3, 0.8));
        // screw holes M2 clearance (aligned with bosses)
        for (x=[2.5, in_l-2.5], y=[2.5, in_w-2.5])
            translate([wall+x, wall+y, -0.1])
                cylinder(h=4, d=2.2);
    }
}

// ---- Render switch ----
part = "body";   // "body" | "cover" | "both"
if (part == "body") body();
if (part == "cover") cover();
if (part == "both") { body(); translate([0, case_w+10, 0]) cover(); }
