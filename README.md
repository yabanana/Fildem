# Fildem Rewrite

Fildem is now a full rewrite focused on GNOME 48+.

## Architecture

1. `src/core` - C/GObject shared core (`libfildem-core`)
2. `src/daemon` - `fildemd` session daemon (`org.fildem`)
3. `src/extension/fildem-shell@fildem.org` - GNOME Shell extension (ESM)
4. `src/hud` - `fildemhud` GTK4 HUD
5. `interfaces` - versioned D-Bus contracts (`org.fildem.v1.*`)

## Build

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

## Install (local staging)

```bash
meson install -C build --destdir /tmp/fildem-install
```

## Runtime (manual)

```bash
./build/src/daemon/fildemd
./build/src/hud/fildemhud
./build/src/ctl/fildemctl status
```

Useful commands:

```bash
./build/src/ctl/fildemctl top-level <window_uid>
./build/src/ctl/fildemctl menu-tree <window_uid>
./build/src/ctl/fildemctl actions <window_uid>
./build/src/ctl/fildemctl hud-request <window_uid>
./build/src/ctl/fildemctl hud-query <window_uid> <term>
./build/src/ctl/fildemctl hud-exec <window_uid> <entry_id>
```

## API documentation

- `docs/DBUS_API_V1.md`
- `docs/REWRITE_PLAN_IT.md`
- `docs/IMPLEMENTATION_PLAN_REAL.md`

## Legacy

Legacy fork code was intentionally removed from the active tree.
The legacy baseline is preserved in git tag `legacy/fork-last-2026-03-24`.
