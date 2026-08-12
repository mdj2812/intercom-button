/*
 * ESP32-S3 desktop intercom enclosure
 *
 * Hardware:
 *   - ESP32-S3-DevKitC-1: 62.74 x 25.40 mm
 *   - MAX9814 breakout:   25.00 x 14.00 mm, 9 mm microphone can
 *   - 22 mm panel-mount momentary button
 *
 * Set `part` to "base" or "lid", render (F6), then export STL.
 * The default "assembly" view includes translucent component mockups.
 */

$fn = 64;

part = "assembly"; // "assembly", "base", "lid", or "print"
show_components = true;
assembly_shell_alpha = 0.28;
selected_part = is_undef(render_part) ? part : render_part;

// Overall enclosure
case_w = 80;
case_d = 98;
front_h = 38;
rear_h = 44;
corner_r = 7;
wall = 2.4;
top_t = 3.2;
floor_t = 2.8;
split_z = 12;
fit = 0.30;

// Closure: four pairs of 6 x 2 mm neodymium magnets
magnet_x = 6.7;
magnet_y = 7.0;
magnet_d = 6;
magnet_h = 2;
magnet_fit = 0.2;
magnet_pocket_d = magnet_d + magnet_fit;
magnet_pocket_h = magnet_h + magnet_fit;
magnet_boss_d = 9;
magnet_right_x = 74;
magnet_points = [
    [magnet_x, magnet_y],
    [magnet_right_x, magnet_y],
    [magnet_x, case_d - magnet_y],
    [magnet_right_x, case_d - magnet_y]
];

// Button
button_hole_d = 22.2;
button_cap_d = 29;
button_recess_d = 30.5;
button_recess_depth = 1.2;
button_x = 17.5;
button_y = 23;

// ESP32-S3-DevKitC-1
esp_w = 25.40;
esp_l = 62.74;
esp_t = 1.6;
esp_rear_clearance = 2.2;
esp_y =
    case_d - wall - fit - esp_rear_clearance - esp_l;
esp_module_overhang = 3.5;
esp_header_pitch = 2.54;
esp_header_count = 22;
esp_header_row_spacing = 22.86;
esp_header_y = esp_y + 2.5;

breadboard_w = 55;
breadboard_l = 82.5;
breadboard_h = 8.5;
breadboard_x = 41.5;
breadboard_hole_rows = 30;
breadboard_header_y_index = 7;
breadboard_hole_y_margin =
    (breadboard_l
        - (breadboard_hole_rows - 1) * esp_header_pitch) / 2;
breadboard_y =
    esp_header_y - breadboard_hole_y_margin
        - breadboard_header_y_index * esp_header_pitch;
breadboard_z = floor_t;
esp_x = breadboard_x;
male_header_spacer = 2.5;
esp_z = breadboard_z + breadboard_h + male_header_spacer;

// Separate rear openings leave a center bridge between the two USB ports.
usb_port_w = 8.5;
usb_center_gap = 4.2;
usb_h = 4.2;
usb_z = esp_z + esp_t - 0.8;

// MAX9814 vertical pocket behind the front grille
mic_w = 25;
mic_h = 14;
mic_t = 1.6;
mic_clearance = 0.6;
mic_can_d = 9.7;
mic_can_h = 9;
mic_can_edge_offset = 7;
mic_x = 54;
mic_z = 23.5;
mic_board_x = mic_x - (mic_w / 2 - mic_can_edge_offset);
mic_board_y = wall + mic_can_h + 1;

slope = (rear_h - front_h) / case_d;
slope_angle = atan(slope);

module rounded_2d(w, d, r) {
    hull() {
        for (x = [r, w - r])
            for (y = [r, d - r])
                translate([x, y]) circle(r = r);
    }
}

module rounded_prism(w, d, h, r) {
    linear_extrude(height = h)
        rounded_2d(w, d, r);
}

module rounded_wedge(w, d, hf, hr, r) {
    cut_angle = atan((hr - hf) / d);

    // Cut a tall rounded prism with the lower face of a rotated cube.
    // This avoids non-planar facets produced by some WebAssembly renderers
    // when a sloped polyhedron participates in nested boolean operations.
    difference() {
        rounded_prism(w, d, hr + 0.1, r);
        translate([-1, 0, hf])
            rotate([cut_angle, 0, 0])
                cube([w + 2, d + hr + 10, hr + 10]);
    }
}

module magnet_positions() {
    for (p = magnet_points)
        translate([p[0], p[1], 0]) children();
}

module usb_cutout(z0, height) {
    // Cut through the rear shell wall.
    cut_y = case_d - wall - 1;
    for (x = [
        esp_x - usb_center_gap / 2 - usb_port_w,
        esp_x + usb_center_gap / 2
    ])
        translate([x, cut_y, z0])
            cube([
                usb_port_w,
                case_d - cut_y + 1,
                height
            ]);
}

module base_magnet_bosses() {
    magnet_positions()
        translate([0, 0, floor_t])
            cylinder(h = split_z - floor_t, d = magnet_boss_d);
}

module base_magnet_pockets() {
    magnet_positions()
        translate([0, 0, split_z - magnet_pocket_h])
            cylinder(
                h = magnet_pocket_h + 0.2,
                d = magnet_pocket_d
            );
}

module base() {
    difference() {
        union() {
            difference() {
                rounded_prism(case_w, case_d, split_z, corner_r);
                translate([wall, wall, floor_t])
                    rounded_prism(
                        case_w - 2 * wall,
                        case_d - 2 * wall,
                        split_z - floor_t + 1,
                        max(0.1, corner_r - wall)
                    );
            }
            base_magnet_bosses();
        }

        base_magnet_pockets();
        usb_cutout(usb_z, usb_h);
    }
}

module upper_outer_shell() {
    intersection() {
        rounded_wedge(case_w, case_d, front_h, rear_h, corner_r);
        translate([-1, -1, split_z])
            cube([case_w + 2, case_d + 2, rear_h - split_z + 2]);
    }
}

module upper_inner_void() {
    inner_d = case_d - 2 * wall;
    inner_front_h = front_h + slope * wall - top_t;
    inner_rear_h = front_h + slope * (case_d - wall) - top_t;

    intersection() {
        translate([wall, wall, 0])
            rounded_wedge(
                case_w - 2 * wall,
                inner_d,
                inner_front_h,
                inner_rear_h,
                max(0.1, corner_r - wall)
            );
        translate([wall, wall, split_z - 0.1])
            cube([
                case_w - 2 * wall,
                case_d - 2 * wall,
                rear_h - split_z + 1
            ]);
    }
}

module lid_magnet_bosses() {
    magnet_positions()
        translate([0, 0, split_z])
            cylinder(h = 6, d = magnet_boss_d);
}

module lid_magnet_pockets() {
    magnet_positions()
        translate([0, 0, split_z - 0.1])
            cylinder(
                h = magnet_pocket_h + 0.2,
                d = magnet_pocket_d
            );
}

module button_cutout() {
    z_top = front_h + slope * button_y;
    normal = [0, -sin(slope_angle), cos(slope_angle)];

    translate([
        button_x - normal[0] * 9,
        button_y - normal[1] * 9,
        z_top - normal[2] * 9
    ])
        rotate([slope_angle, 0, 0])
            cylinder(h = 18, d = button_hole_d);
}

module button_recess() {
    z_top = front_h + slope * button_y;
    normal = [0, -sin(slope_angle), cos(slope_angle)];
    cut_h = button_recess_depth + 0.2;

    // Circular counterbore normal to the sloped top surface.
    translate([
        button_x - normal[0] * button_recess_depth,
        button_y - normal[1] * button_recess_depth,
        z_top - normal[2] * button_recess_depth
    ])
        rotate([slope_angle, 0, 0])
            cylinder(h = cut_h, d = button_recess_d);
}

module mic_grille_cutouts() {
    hole_d = 2.2;
    spacing = 4.4;
    points = [
        [0, 0],
        [-spacing, 0], [spacing, 0],
        [-spacing / 2, spacing], [spacing / 2, spacing],
        [-spacing / 2, -spacing], [spacing / 2, -spacing]
    ];

    for (p = points)
        translate([mic_x + p[0], wall + 1, mic_z + p[1]])
            rotate([90, 0, 0])
                cylinder(h = wall + 2, d = hole_d);
}

module mic_pocket() {
    pocket_w = mic_w + 2 * mic_clearance;
    pocket_h = mic_h + 2 * mic_clearance;
    x0 = mic_board_x - pocket_w / 2;
    z0 = mic_z - pocket_h / 2;
    rail_t = 1.4;
    lip = 1.2;
    slot_front = mic_board_y - mic_clearance;
    slot_back = mic_board_y + mic_t + mic_clearance;
    support_depth = slot_back + rail_t - wall;

    // Side supports reach back far enough for the 9 mm microphone can.
    for (x = [x0 - rail_t, x0 + pocket_w])
        translate([x, wall, z0])
            cube([rail_t, support_depth, pocket_h]);

    // Short front/back lips form two vertical U-channels. The PCB slides
    // upward from the open bottom while leaving its center components clear.
    for (x = [x0 - rail_t, x0 + pocket_w - lip])
        for (y = [slot_front - rail_t, slot_back])
            translate([x, y, z0])
                cube([rail_t + lip, rail_t, pocket_h]);

    // Top stop joins the rails to the front wall and limits insertion depth.
    translate([x0 - rail_t, wall, z0 + pocket_h])
        cube([
            pocket_w + 2 * rail_t,
            support_depth,
            rail_t
        ]);

    // One wide central cantilever latch leaves both PCB ends clear for
    // soldered wires. Its tapered hook flexes rearward during insertion.
    board_bottom = mic_z - mic_h / 2;
    snap_w = 10;
    snap_t = 0.8;
    snap_hook = 1.4;
    snap_hook_h = 1.2;
    snap_x = mic_board_x - snap_w / 2;
    snap_y = slot_back + 0.2;
    snap_bottom = board_bottom - 0.2;
    snap_top = z0 + pocket_h + 0.2;

    translate([snap_x, snap_y, snap_bottom])
        cube([
            snap_w,
            snap_t,
            snap_top - snap_bottom
        ]);

    hull() {
        translate([
            snap_x,
            snap_y - snap_hook,
            snap_bottom - snap_hook_h
        ])
            cube([snap_w, snap_t + snap_hook, 0.2]);
        translate([snap_x, snap_y, snap_bottom - 0.1])
            cube([snap_w, snap_t, 0.2]);
    }
}

module lid() {
    difference() {
        union() {
            difference() {
                upper_outer_shell();
                upper_inner_void();
            }
            lid_magnet_bosses();
            mic_pocket();
        }

        button_cutout();
        button_recess();
        mic_grille_cutouts();
        usb_cutout(usb_z, usb_h);
        lid_magnet_pockets();
    }
}

module esp32_mockup() {
    board_x0 = esp_x - esp_w / 2;
    board_y1 = esp_y + esp_l;
    module_y = esp_y - esp_module_overhang;
    breadboard_x0 = breadboard_x - breadboard_w / 2;
    breadboard_hole_y0 = breadboard_y + breadboard_hole_y_margin;
    header_xs = [
        esp_x - esp_header_row_spacing / 2,
        esp_x + esp_header_row_spacing / 2
    ];

    // Half-size solderless breadboard and representative terminal holes
    color([0.93, 0.93, 0.90])
        translate([breadboard_x0, breadboard_y, breadboard_z])
            cube([breadboard_w, breadboard_l, breadboard_h]);
    color([0.72, 0.72, 0.68])
        translate([
            breadboard_x - 2.5,
            breadboard_y,
            breadboard_z + breadboard_h
        ])
            cube([5, breadboard_l, 0.2]);
    for (row = [0 : breadboard_hole_rows - 1])
        for (side = [-1, 1])
            for (column = [0 : 4])
                color([0.10, 0.10, 0.10])
                    translate([
                        breadboard_x
                            + side * (3.81
                                + column * esp_header_pitch),
                        breadboard_hole_y0
                            + row * esp_header_pitch,
                        breadboard_z + breadboard_h
                    ])
                        cylinder(h = 0.2, d = 1.0, $fn = 10);

    // PCB
    color([0.04, 0.42, 0.18])
        translate([board_x0, esp_y, esp_z])
            cube([esp_w, esp_l, esp_t]);

    // Male-header plastic and pins inserted into the breadboard
    for (x = header_xs) {
        color([0.04, 0.04, 0.04])
            translate([
                x - 1.0,
                esp_header_y - 1.0,
                esp_z - male_header_spacer
            ])
                cube([
                    2.0,
                    (esp_header_count - 1)
                        * esp_header_pitch + 2.0,
                    male_header_spacer
                ]);

        for (i = [0 : esp_header_count - 1])
            color([0.72, 0.72, 0.68])
                translate([
                    x,
                    esp_header_y + i * esp_header_pitch,
                    breadboard_z + breadboard_h - 4
                ])
                    cylinder(
                        h = esp_z
                            - (breadboard_z + breadboard_h) + 4.2,
                        d = 0.65,
                        $fn = 12
                    );
    }

    // WROOM module: exposed antenna area and shield can
    color([0.10, 0.17, 0.12])
        translate([esp_x - 9, module_y, esp_z + esp_t])
            cube([18, 7, 1.2]);
    color([0.70, 0.72, 0.70])
        translate([esp_x - 9, module_y + 7, esp_z + esp_t])
            cube([18, 18, 3]);

    // Two Micro-USB receptacles
    for (x = [
        esp_x - usb_center_gap / 2 - usb_port_w / 2,
        esp_x + usb_center_gap / 2 + usb_port_w / 2
    ])
        color([0.68, 0.70, 0.72])
            translate([
                x - 3.75,
                board_y1 - 5.5,
                esp_z + esp_t
            ])
                cube([7.5, 7, 3.0]);
}

module mic_mockup() {
    // PCB
    color([0.28, 0.08, 0.42])
        translate([
            mic_board_x - mic_w / 2,
            mic_board_y,
            mic_z - mic_h / 2
        ])
            cube([mic_w, mic_t, mic_h]);

    // Electret microphone can and acoustic face
    color([0.70, 0.71, 0.70])
        translate([mic_x, mic_board_y, mic_z])
            rotate([90, 0, 0])
                cylinder(h = mic_can_h, d = mic_can_d);
    color([0.08, 0.08, 0.08])
        translate([
            mic_x,
            mic_board_y - mic_can_h - 0.15,
            mic_z
        ])
            rotate([90, 0, 0])
                cylinder(h = 0.3, d = mic_can_d - 0.5);
}

module button_mockup() {
    z_top = front_h + slope * button_y;
    normal = [0, -sin(slope_angle), cos(slope_angle)];
    body_depth = 18.9;

    // Threaded barrel below the panel
    color([0.12, 0.12, 0.12])
        translate([
            button_x - normal[0] * body_depth,
            button_y - normal[1] * body_depth,
            z_top - normal[2] * body_depth
        ])
            rotate([slope_angle, 0, 0])
                cylinder(h = body_depth, d = 21.8);

    // Retaining nut below the panel
    color([0.20, 0.20, 0.20])
        translate([
            button_x - normal[0] * 4,
            button_y - normal[1] * 4,
            z_top - normal[2] * 4
        ])
            rotate([slope_angle, 0, 0])
                cylinder(h = 4, d = 26, $fn = 6);

    // Bezel seated in the top counterbore
    color([0.12, 0.12, 0.12])
        translate([button_x, button_y, z_top])
            rotate([slope_angle, 0, 0])
                cylinder(h = 3, d = button_cap_d);

    // Moving button cap
    color([0.10, 0.72, 0.20])
        translate([
            button_x + normal[0] * 3,
            button_y + normal[1] * 3,
            z_top + normal[2] * 3
        ])
            rotate([slope_angle, 0, 0])
                cylinder(h = 16, d = 24.8);
}

module component_mockups() {
    esp32_mockup();
    mic_mockup();
    button_mockup();
}

if (selected_part == "base") {
    base();
} else if (selected_part == "lid") {
    lid();
} else if (selected_part == "print") {
    base();
    translate([case_w + 12, 0, 0]) lid();
} else {
    shell_alpha = show_components ? assembly_shell_alpha : 1;
    color([0.82, 0.84, 0.87, shell_alpha]) base();
    color([0.93, 0.94, 0.96, shell_alpha]) lid();
    if (show_components) component_mockups();
}
