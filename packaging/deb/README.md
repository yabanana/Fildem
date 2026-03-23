# Debian Packaging (Rewrite)

Target binary packages:

1. `fildemd` (daemon + interfaces)
2. `fildemhud` (GTK4 HUD)
3. `gnome-shell-extension-fildem-shell` (UUID `fildem-shell@fildem.org`)

Expected install assets:

1. `/usr/bin/fildemd`
2. `/usr/bin/fildemhud`
3. `/usr/share/fildem/interfaces/org.fildem.v1.*.xml`
4. `/usr/share/gnome-shell/extensions/fildem-shell@fildem.org/`
5. `/usr/share/applications/org.fildem.hud.desktop`
6. `/usr/lib/systemd/user/fildemd.service`
