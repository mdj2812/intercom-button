// Part 2: shell — wedge case via hull of 4 corner posts
include <params.scad>

h_front = in_h - tan(top_tilt)*in_l/2;   // interior top at front
h_back  = in_h + tan(top_tilt)*in_l/2;   // interior top at back

module corner_posts(l, w, hf, hb, r) {
    // 4 vertical posts; hull() them => wedge with sloped top
    for (y=[r, w-r]) {
        translate([r,   y, 0]) cylinder(h=hf, r=r);
        translate([l-r, y, 0]) cylinder(h=hb, r=r);
    }
}

function top_z(x) = wall + h_front + (h_back-h_front)*x/in_l;

module shell() {
    top_t = 2;   // top thickness
    difference() {
        // outer solid (bottom wall + interior height)
        translate([0, 0, 0])
            hull() corner_posts(case_l, case_w,
                                wall+h_front, wall+h_back, corner_r);
        // interior cavity (stops top_t below the sloped top)
        translate([wall, wall, wall])
            hull() corner_posts(in_l, in_w,
                                h_front-top_t, h_back-top_t,
                                max(corner_r-wall, 1));
        // open bottom (for separate cover plate)
        translate([wall, wall, -0.1])
            hull() corner_posts(in_l, in_w, wall+0.2, wall+0.2,
                                max(corner_r-wall, 1));
    }
}
