# Enclosure — 3D Printed Case

Parametric OpenSCAD design for the intercom button (breadboard-free assembly).

## Files

| File | Description |
|------|-------------|
| `params.scad` | All dimensions — edit here to tweak |
| `shell.scad` | Wedge shell (sloped top via hull) |
| `features.scad` | Button hole, USB cutout, mic holes, cradle, bosses |
| `main.scad` | Assembly + render switch |
| `case_body.stl` | Main body, ready to print |
| `case_cover.stl` | Bottom cover, ready to print |

## Specs

- **Outer**: 92 × 48 × 30~44 mm (8° sloped top)
- **Button**: 22 mm panel mount (M22 thread), hole drilled **normal to the
  sloped top** — bezel sits flush, button tilts 8° toward the user
- **MCU**: ESP32-S3-DevKitC on cradle rails, USB-C out the back (11 × 6 mm cutout)
- **Mic**: MAX9814 slides between 4 ceiling pegs (aligned under the
  5 × 1.2 mm sound holes), fix with double-sided tape
- **Status LED**: 3 mm light-pipe hole above DevKitC RGB LED
- **Cover**: 1.6 mm plate, 4 × M2 self-tapping screws into corner bosses

## Print (JLC / 嘉立创)

- Body: `case_body.stl`, **open side down** (flat rim on bed) — no supports
- Cover: `case_cover.stl`, flat — no supports
- Material: SLA resin (9000R white) or SLS nylon; 29 + 6 cm³ ≈ ¥40-80

## Render / export

```bash
openscad -o case_body.stl -D 'part="body"' main.scad
openscad -o case_cover.stl -D 'part="cover"' main.scad
# previews (headless server):
xvfb-run -a openscad -o preview.png --autocenter --viewall \
  --camera=0,0,0,55,0,25,500 --imgsize=900,650 -D 'part="body"' main.scad
```
