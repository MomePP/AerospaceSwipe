# Release process

Releases are cut by hand — there is no CI release workflow (the unused one was
dropped in `20ae143`).

## Steps

1. **Bump `VERSION` in `makefile`**, commit as `release: vX.Y.Z`. That single
   variable feeds `CFBundleShortVersionString` and `CFBundleVersion` in the
   generated Info.plist; there is no other version string to update.
2. **Tag and push**: `git tag -a vX.Y.Z`, `git push origin main`,
   `git push origin vX.Y.Z`.
3. **Bump the tap formula** — `aerospace-swipe.rb` in
   `MomePP/homebrew-formulae`, both `tag:` and `revision:` (the revision is the
   commit the tag points at). Commit as `aerospace-swipe: bump to vX.Y.Z`.
4. **Create the GitHub release** for the tag. No build artifacts are attached —
   the formula builds from source.
5. `brew update && brew upgrade aerospace-swipe`, then **re-sign** (below), then
   `brew services restart aerospace-swipe`.

## Edit the tap in a separate clone

Do **not** edit `/opt/homebrew/Library/Taps/momepp/homebrew-formulae` in place —
that is the live tap, and `brew update` can reset it. Clone the repo somewhere
scratch, edit, commit, push, then `brew update` to pick it up.

(This was violated once, during the v1.0.2 release: the live tap was edited
directly. It survived because the change was committed and pushed immediately,
and the tap was verified clean and level with the remote afterwards — but the
convention exists for a reason and shouldn't be relied on to work again.)

## Re-signing is required after every upgrade

Homebrew builds the app **ad-hoc signed** — its install sandbox can't reach the
keychain — so macOS invalidates Accessibility permission on every
`brew upgrade`. The prompt reappears and the app vanishes from System Settings ›
Privacy & Security › Accessibility.

Fix, after each upgrade:

```bash
codesign --force --sign "AerospaceSwipe Local Signing" \
  --entitlements /opt/homebrew/opt/aerospace-swipe/accessibility.entitlements \
  /opt/homebrew/opt/aerospace-swipe/AerospaceSwipe.app
brew services restart aerospace-swipe
```

`./setup-codesign-identity.sh` creates that local identity once. Because the
signing identity and bundle id stay the same, the designated requirement still
matches and the Accessibility grant survives — verify with
`Accessibility permission granted` in `/opt/homebrew/var/log/aerospace-swipe.log`.

The same recipe is in the formula's `caveats`, which is where a user without
this file would find it.

## Don't hand-place builds into the Cellar

Copying a locally built `AerospaceSwipe.app` over
`/opt/homebrew/opt/aerospace-swipe/` works for testing, but the Cellar still
records the old version — so the next `brew upgrade` silently reverts to it.
Fine for a test cycle; always follow up with a real tagged release.

## Log buffering

`switch_workspace()` reports success via `printf`, and stdout is block-buffered
when redirected to a file. Swipe lines therefore sit in a ~4KB buffer and the
log's mtime can stay at process start for a long time even while swiping works.
**Absence of recent log lines is not evidence the service is broken.** A
truncated final line (e.g. `Swit`) just means the process was killed mid-buffer.

A `setvbuf(stdout, NULL, _IOLBF, 0)` in `main()` would make the log
line-buffered and far more useful for diagnosis; not done yet.
