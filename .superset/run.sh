#!/bin/bash
# Build and run the swipe daemon in the foreground, in this workspace only.
#
# Deliberately not `make install`: that writes a launch agent into
# ~/Library/LaunchAgents, which is global state a throwaway workspace must not
# touch. `make build` = compile + codesign with the accessibility entitlement,
# which is all a foreground run needs.
#
# swipe holds an fcntl lock on /tmp/aerospace-swipe-$USER.lock, so if the
# installed service (brew services / launchd) is already running, this exits
# immediately with "already running?" instead of double-switching workspaces.
# Stop the service first (`brew services stop aerospace-swipe` or
# `make unload_plist`) to test a build from here.
#
# First foreground run will prompt for Accessibility permission. Ad-hoc
# signatures change on every rebuild, so the grant won't stick — run
# ./setup-codesign-identity.sh once (it's interactive, hence not in setup) to
# get a stable local identity the makefile picks up automatically.
set -euo pipefail

make build

# exec so the pane's stop signal reaches swipe itself, not a bash parent.
exec ./swipe
