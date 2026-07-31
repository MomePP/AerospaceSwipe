# App icon

`Resources/AppIcon.icns`, wired into the bundle by the makefile's `bundle`
target (`CFBundleIconFile` + `CFBundleIconName`, both `AppIcon`, matching how
AeroSpace's own Info.plist does it).

## Design

Pairs visually with AeroSpace's icon without copying it: the same three-circle
family on a white rounded square with a soft drop shadow, but with swipe
chevrons in place of AeroSpace's window-op glyphs (−, ×, +), since this app
switches workspaces by trackpad rather than tiling.

Colours sampled directly from `/Applications/AeroSpace.app`'s `AppIcon.icns`:

| Element | Colour |
| --- | --- |
| yellow circle | `#F3C04B` |
| coral circle | `#EC6C5D` |
| green circle | `#60C652` |

Each glyph is the same hue as its circle at ~42% brightness, which is how
AeroSpace derives its own glyph colours. Left chevron (‹) on coral, right
chevron (›) on green, and a horizontal 3-dot "steps" glyph on yellow, nodding to
multi-step swiping. Overlap and positioning mirror AeroSpace's layout.

## Regenerating it

No design tool was involved — it's drawn programmatically with Pillow:

1. Render at 1024×1024 (rounded-square background, drop shadow, three circles,
   three glyphs) using bezier/arc primitives, anti-aliased via 4× supersampling.
2. Downsample into an `.iconset` folder at the ten sizes `iconutil` expects
   (16/32/128/256/512, each @1x and @2x).
3. `iconutil -c icns` → `AppIcon.icns`.

**Only the final `.icns` is committed** — not the generator script, the
`.iconset` folder, or the intermediate PNGs. If you need to regenerate, the
generator has to be rewritten from this description; that was a deliberate
trade rather than an oversight.
