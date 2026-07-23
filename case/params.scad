// Intercom Button Enclosure — parametric
// ESP32-S3-DevKitC + MAX9814 + 22mm panel button
// Units: mm. Part 1: parameters.

// ---- Button (22mm panel mount) ----
btn_hole_d   = 22.4;   // panel cutout (M22 thread + clearance)
btn_cap_d    = 24.8;   // cap diameter
btn_body_d   = 26;     // body below panel + clearance
btn_body_len = 32;     // body + pins clearance below panel

// ---- ESP32-S3-DevKitC-1 ----
pcb_l = 51;      // PCB length (incl USB-C overhang)
pcb_w = 26;      // PCB width
pcb_h = 11;      // height above carrier (pins+USB)

// ---- Case ----
wall = 2;        // wall thickness
in_l = 88;       // interior length (X, front-back)
in_w = 44;       // interior width  (Y, left-right)
in_h = 36;       // interior height (Z)
corner_r = 4;    // outer corner radius
top_tilt = 8;    // top slope angle (deg), high at back

// ---- Derived ----
case_l = in_l + 2*wall;
case_w = in_w + 2*wall;

// Button position: front area, centered left-right
btn_x = 20;      // from interior front wall
btn_y = in_w/2;

// PCB cradle: rear area
pcb_x0 = 34;     // PCB front edge from interior front
pcb_y0 = (in_w - pcb_w)/2;

// Mic position (top, near button, side)
mic_x = 40;
mic_y = in_w/2;

$fn = 48;
