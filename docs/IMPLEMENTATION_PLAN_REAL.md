# Fildem Rewrite - Piano di Implementazione Reale

Data riferimento: 24 marzo 2026

## Obiettivo di delivery

Portare la riscrittura a un prodotto completo su stack nuovo (`C/GIO + GNOME Shell ESM + GTK4`) con:

1. menu top-level affidabile nel panel
2. attivazione voci menu e azioni finestra
3. HUD funzionante su API D-Bus v1
4. percorso Wayland-first con fallback controllati
5. build, test e packaging riproducibili

## Stato implementazione per fase

### Fase A - Specifica legacy -> target

Stato: completata a livello di analisi (`docs/REWRITE_PLAN_IT.md`).

Output operativo:

1. scope legacy esplicitato
2. keep/drop/redesign formalizzato
3. ordine di migrazione definito

### Fase B - Confini responsabilita'

Stato: completata.

Output operativo:

1. Extension: stato Shell + UI panel + shell actions
2. Daemon: bus owner + provider menu + cache/registry + activation router
3. Core: validazione/serializzazione/cache/query model
4. HUD: client GTK4 disaccoppiato

### Fase C - API D-Bus v1

Stato: completata.

Artefatti:

1. `interfaces/org.fildem.v1.Window.xml`
2. `interfaces/org.fildem.v1.TopLevel.xml`
3. `interfaces/org.fildem.v1.MenuTree.xml`
4. `interfaces/org.fildem.v1.Activation.xml`
5. `interfaces/org.fildem.v1.WindowActions.xml`
6. `interfaces/org.fildem.v1.Hud.xml`

### Fase D - Extension GNOME 48+ ESM

Stato: completata per MVP/V2 core.

Implementato:

1. lifecycle extension + panel controller
2. tracking finestra attiva con context esteso (xid + gtk exporter paths)
3. bridge D-Bus completo (`UpdateActiveWindow`, top-level, actions)
4. window actions shell-native complete (list + execute)
5. sync azioni verso daemon (`SetWindowActions`)
6. handling `ActivationRequested` per execute action da HUD/daemon
7. shortcut HUD configurabile (`hud-shortcut`)

### Fase E - Core C/GObject/GIO + daemon

Stato: completata per MVP/V2 core.

Implementato:

1. `libfildem-core` (error, serializer, registry, cache, menu query model)
2. `fildemd` con owner `org.fildem`
3. parser/provider GTK exporter (`org.gtk.Menus`) e DBusMenu
4. routing attivazioni (`ActivateTopLevel`, `ActivateMenuItem`, `Execute`)
5. cache/menu publish su `TopLevelChanged` e `MenuTreeChanged`

### Fase F - HUD GTK4

Stato: completata MVP.

Implementato:

1. `fildemhud` GTK4 standalone
2. query live su `org.fildem.v1.Hud.Query`
3. execute entry su `org.fildem.v1.Hud.Execute`
4. trigger remoto via `org.fildem.v1.Hud.RequestHud` / `HudRequested`

### Fase G - Shortcut system moderno

Stato: parzialmente completata (path operativo pronto).

Implementato:

1. keybinding gestito dalla shell extension (`hud-shortcut`)
2. fallback robusto: launch `fildemhud` dal binding

Backlog immediato per parity avanzata:

1. backend `org.freedesktop.portal.GlobalShortcuts` dedicato
2. gestione capability/runtime fallback automatici portal -> extension

### Fase H - Test harness

Stato: completata base, estensione in corso.

Implementato:

1. unit test core (`tests/test-core.c`)
2. contract test introspection XML (`tests/test-contract.c`)
3. `meson test` integrato in build stack

Backlog immediato:

1. smoke D-Bus con `dbus-run-session`
2. integration tests su sessione GNOME nested

## Piano esecutivo prossimo rilascio

## R1 - Stabilizzazione IPC/UI (pronto)

1. chiudere bugfix minori extension
2. verifiche manuali su GNOME 48/49
3. freeze API v1

## R2 - GlobalShortcuts portal

1. modulo dedicated shortcut backend
2. registrazione/attivazione shortcut portal
3. fallback automatico su keybinding extension

## R3 - Test integrazione e packaging finale

1. test smoke automatizzati in `dbus-run-session`
2. test nested-shell in CI
3. pacchetti Arch/Fedora/Deb aggiornati su stack nuovo

## Definizione di done (release complete)

1. top-level e activate funzionanti su app GTK/DBusMenu principali
2. window actions complete e execute da panel/HUD
3. HUD launch/query/execute via shortcut
4. `meson compile && meson test` verdi in CI
5. pacchetti installabili con service utente `fildemd`
