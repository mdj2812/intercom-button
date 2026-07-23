# Desktop Intercom Button Enclosure

English | [简体中文](README.zh-CN.md)

This is the first parametric OpenSCAD enclosure design for:

- ESP32-S3-DevKitC-1 (official size: 62.74 × 25.40 mm)
- MAX9814 microphone module (25 × 14 mm PCB, 9 mm microphone can height)
- 22 mm panel-mount momentary push button

## Dimensions

- Footprint: 98 × 68 mm
- Front height: 38 mm
- Rear height: 44 mm
- Top slope: approximately 3.5°
- Wall thickness: 2.4 mm
- Top thickness: 3.2 mm (2.0 mm remains below the button recess)
- Button opening: Ø22.2 mm with a Ø30.5 × 1.2 mm counterbore
- Shell alignment and closure: four pairs of Ø6 × 2 mm neodymium magnets, without an alignment skirt
- Clearance below the ESP32 PCB: approximately 8.2 mm for headers and wiring
- USB openings: two separate 10 × 5 mm openings with a 3 mm center retention bridge

The enclosure has separate base and lid parts. The ESP32 mounts toward the right side of the base with its USB ports facing rearward. The button sits at the front-left to keep its metal body away from the ESP32 antenna. The MAX9814 mounts vertically behind a seven-hole grille in the front panel.

## Generating STL Files

Keep these three SCAD files in the same directory:

- `intercom_button_base.scad`: base export entry point; render with `F6`, then export as STL
- `intercom_button_lid.scad`: lid export entry point; render with `F6`, then export as STL
- `intercom_button.scad`: shared parameters, geometry modules, and assembly preview

Change dimensions only in the shared file so the base and lid remain synchronized.

### GitHub Actions

`.github/workflows/enclosure-stl.yml` generates both STL files when enclosure files change in a push or pull request, or when the workflow is run manually. It uploads an `intercom-button-enclosure-stl` artifact containing the STL files and `SHA256SUMS`, retained for 14 days.

## Recommended Print Settings

- Material: PETG recommended; the ESP32 and microphone clips are not suitable for repeated flexing when printed in PLA
- Nozzle: 0.4 mm
- Layer height: 0.20 mm
- Walls: 3 perimeters
- Infill: 15%–20%
- Base: print bottom face down
- Lid: let the slicer select the best orientation; localized support may be required near the button opening

## Assembly

1. Place the ESP32 in the base cradle with its USB ports aligned to the rear openings.
2. Insert the USB end at an angle beneath the center tab between the two USB ports and against the rear stop. Press down the antenna end until the two opposing clips engage the top of the PCB.
3. Push the MAX9814 upward into the lid rails until the integrated latch engages. Align the microphone can with the grille. To remove it, gently deflect the single latch.
4. Insert the button through the Ø22.2 mm top opening and secure it with its supplied nut.
5. Arrange all eight Ø6 × 2 mm magnets and mark their polarities.
6. Bond the magnets into the Ø6.2 × 2.2 mm blind pockets with epoxy, ensuring that every opposing pair attracts.
7. Allow the epoxy to cure fully before closing the enclosure. To open it, carefully separate the shells at the USB openings.

## First Test Fit

Compatible boards from different manufacturers may vary by 1–3 mm from the Espressif reference design. Before the final print, verify:

- The development board length, width, and combined USB connector spacing
- The MAX9814 PCB dimensions and microphone position
- That the button barrel passes through a Ø22.2 mm test opening
- Your printer's hole-size and shell-fit tolerances

Dimension references:

- [Official Espressif ESP32-S3-DevKitC-1 dimension drawing](https://dl.espressif.com/dl/PCB_ESP32-S3-DevKitC-1_V1_20210312CB.pdf)
- [Adafruit MAX9814 reference design](https://www.adafruit.com/product/1713) (third-party PCB dimensions may differ)
