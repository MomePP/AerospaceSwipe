# AerospaceSwipe — aerospace workspace switching with trackpad swipes

<img src="Resources/AppIcon.png" width="96" align="right" alt="AerospaceSwipe icon">

AerospaceSwipe detects x-fingered(defaults to 3) swipes on your trackpad and correspondingly switches between [aerospace](https://github.com/nikitabobko/AeroSpace) workspaces.

> originally forked from [acsandmann/aerospace-swipe](https://github.com/acsandmann/aerospace-swipe), now maintained
> as a standalone repository — all credit for the original project goes to [@acsandmann](https://github.com/acsandmann).
> on top of it this adds stable per-finger touch tracking, continuous multi-step swiping, adjustable sensitivity,
> multi-monitor targeting, scroll suppression, a menu bar with runtime controls, an app icon, and a homebrew tap.
> see [acknowledgements](#acknowledgements).

## features
- fast swipe detection and forwarding to aerospace (uses aerospace server's socket instead of cli)
- works with any number of fingers (default is 3, can be changed in config)
- adjustable sensitivity (Low/Medium/High)
- multi-step swiping: cross more than one workspace in a single continuous gesture, switching live as you swipe (can be disabled for one-switch-per-gesture instead, with velocity-based early triggering for fast flicks)
- targets the monitor under the cursor, not just the focused one (can be disabled in config)
- swallows the swipe's scroll stream, so the app under the cursor doesn't also react to it (e.g. Arc's sidebar no longer switches spaces on a 4-finger swipe)
- optional menu bar icon with runtime controls for all settings
- skips empty workspaces (if enabled in config)
- ignores your palm if it is resting on the trackpad
- haptics on swipe (this is off by default)
- customizable swipe directions (natural or inverted)
- swipe will wrap around workspaces (ex 1-9 workspaces, swipe right from 9 will go to 1)
- utilizes [yyjson](https://github.com/ibireme/yyjson) for performant json ser/de

## configuration
config file is optional and only needed if you want to change the default settings(default settings are shown in the example below)

> to restart after changing the config file: `brew services restart aerospace-swipe` (homebrew install) or
> `make restart` (manual install) — both just unload and reload the launch agent

```jsonc
// ~/.config/aerospace-swipe/config.json
{
  "haptic": false,
  "natural_swipe": false,
  "wrap_around": true,
  "skip_empty": true,
  "fingers": 3,
  "sensitivity": 2,      // 1=Low, 2=Medium, 3=High
  "show_menu_bar": true, // Show menu bar icon with controls
  "multi_swipe": true,   // Cross multiple workspaces in one continuous gesture, live
  "max_steps": 5,        // Cap on workspaces crossed per gesture when multi_swipe is on
  "follow_mouse_monitor": true // Switch the workspace on the monitor under the cursor
}
```

> if you use 3 or 4 fingers, turn off the matching "Swipe between full-screen applications" gesture in
> System Settings › Trackpad › More Gestures, otherwise macOS handles the swipe itself.

## installation
### homebrew (recommended)
```bash
brew tap momepp/formulae
brew install aerospace-swipe        # stable, pinned to the latest tagged release
# or: brew install --HEAD aerospace-swipe   # tracks the main branch instead

brew services start aerospace-swipe # installs + loads the launchd service
```
> config, restart, and uninstall all work the same as the manual install below, except use
> `brew services restart|stop aerospace-swipe` instead of `make restart`/`make uninstall` for the launchd
> service — `brew` owns that plist once installed this way.

### manual
```bash
git clone https://github.com/MomePP/AerospaceSwipe.git
cd AerospaceSwipe
./install.sh # or: make install
```

### keeping accessibility permission across `brew upgrade`
homebrew builds the app ad-hoc signed, so macOS drops its Accessibility grant on every upgrade. to make the grant
survive, create a local signing identity once (from a checkout: `./setup-codesign-identity.sh`), then after each
upgrade re-sign and restart:
```bash
codesign --force --sign "AerospaceSwipe Local Signing" \
  --entitlements /opt/homebrew/opt/aerospace-swipe/accessibility.entitlements \
  /opt/homebrew/opt/aerospace-swipe/AerospaceSwipe.app
brew services restart aerospace-swipe
```
## uninstallation
### homebrew
```bash
brew services stop aerospace-swipe
brew uninstall aerospace-swipe
```
### manual
```bash
cd AerospaceSwipe # wherever you cloned it, e.g. ~/.local/share/aerospace-swipe
./uninstall.sh # or: make uninstall
```

## acknowledgements
- [acsandmann/aerospace-swipe](https://github.com/acsandmann/aerospace-swipe) — the original project this was forked from, by [@acsandmann](https://github.com/acsandmann)
- [nikitabobko/AeroSpace](https://github.com/nikitabobko/AeroSpace) — the tiling window manager this drives
- [ibireme/yyjson](https://github.com/ibireme/yyjson) — json serialization/deserialization
