# Fildem Rewrite Totale (GNOME 48+)

## Premessa

La prossima iterazione di Fildem e' una **riscrittura completa**, non un refactor.
Il codice legacy va usato solo come riferimento comportamentale (feature e aspettative utente), non come base tecnica da evolvere.

## Scope reale della extension legacy

### Funzionalita' oggi coperte

1. Mostrare nel panel i top-level menu dell'app attiva (es. File, Edit, View, Help).
2. Aprire/attivare un menu quando l'utente clicca nel panel.
3. Aggiornarsi al cambio finestra attiva (app, titolo, top-level disponibili, visibilita').
4. Gestire la visibilita' del menu nel panel (hover-only vs sempre visibile) e il label AppMenu.
5. Esporre window actions (minimize, maximize/unmaximize, move/resize, move workspace/monitor, close).
6. Fare da bridge Wayland verso processo esterno per compensare limiti del backend legacy.

### Scope implicito (ma critico)

Nel sistema attuale la extension e' anche:

1. Source of truth dello stato GNOME Shell.
2. Bridge Shell -> backend.
3. Parte del workaround Wayland.

## Scope corretto della nuova extension

### Responsabilita' della nuova extension

1. Osservare stato GNOME Shell:
   - finestra attiva
   - app associata
   - titolo finestra
   - workspace/monitor rilevanti
   - disponibilita' menu esportati
2. Gestire UI panel:
   - rendering top-level
   - behavior hover/click/focus
   - indicatori di stato
   - app label opzionale
3. Agire da adapter Shell -> daemon:
   - invio dati finestra attiva
   - notifica cambi top-level/menu
   - inoltro richieste utente di open/activate
4. Esporre window actions recuperabili dal backend mantenendo la logica shell-specific nella extension.
5. Integrare activation path moderni (HUD, menubar, fallback controllati).

### Responsabilita' da escludere dalla extension

1. Parsing profondo del menu model remoto.
2. Cache complessa del menu tree.
3. Rendering HUD standalone.
4. Process orchestration.
5. Fallback X11-app-centric come percorso primario.

## Architettura target (riscrittura totale)

1. `fildem-shell-extension` (GJS/ESM)
   - modulo GNOME Shell nativo
   - osservazione stato Shell
   - rendering panel menu
   - API bridge verso daemon
2. `libfildem-core` (C/GObject/GIO)
   - libreria core
   - discovery/registrazione menu remoti
   - conversione a modello interno
   - invalidazione cache
   - API D-Bus stabile
3. `fildemd` (daemon di sessione)
   - processo unico
   - bus name interno
   - coordinamento extension, HUD e fonti menu
4. `fildem-hud` (GTK4)
   - UI HUD standalone
   - query/filter/activate
   - disaccoppiato dal panel

## Funzionalita' target

### MVP

1. Extension caricabile su GNOME 48+.
2. Detection affidabile della finestra attiva.
3. Top-level menu nel panel.
4. Click -> open/activate menu corretto.
5. Aggiornamento al cambio finestra.
6. Wayland come percorso primario.
7. Daemon unico.
8. HUD minimale funzionante.

### V2

1. Shortcut globali moderni.
2. Preferences Adwaita-native.
3. Supporto monitor/workspace migliorato.
4. Window actions complete.
5. Cache/invalidazione robusta.
6. Logging e diagnostica avanzati.

### Non-MVP

1. Compatibilita' X11 estesa.
2. Theming/custom CSS avanzato.
3. Backend alternativi toolkit-specific.
4. Feature parity totale con ogni app legacy.

## Backlog di riscrittura

### Fase A - Specifica funzionale dal legacy

**Obiettivo:** estrarre comportamento osservabile senza riuso della struttura tecnica.

Passi:

1. Catalogare feature leggendo:
   - `fildemGMenu@gonza.com/extension.js`
   - `fildem/utils/menu.py`
   - `fildem/utils/window.py`
   - `fildem/handlers/global_menu.py`
   - `fildem/utils/service.py`
2. Separare:
   - comportamento utente
   - workaround tecnologici
   - bug/limiti storici
3. Produrre matrice `legacy -> target -> keep/drop/redesign`.
4. Marcare esplicitamente workaround X11/Wayland da non portare.

Deliverable:

1. Documento di specifica funzionale legacy.
2. Matrice decisionale feature.

### Fase B - Confini di responsabilita'

**Obiettivo:** evitare accoppiamento extension/daemon/core/HUD.

Passi:

1. Definire artefatti:
   - `fildem-shell-extension`
   - `libfildem-core`
   - `fildemd`
   - `fildem-hud`
2. Per ciascuno definire:
   - ownership stato
   - API pubbliche
   - dipendenze consentite
   - eventi in ingresso/uscita
3. Vietare logiche duplicate extension/daemon.
4. Formalizzare payload finestra attiva inviato dalla extension.

Deliverable:

1. Architecture boundaries document.
2. Contratto dati extension -> daemon.

### Fase C - Nuova API D-Bus interna

**Obiettivo:** sostituire metodi/segnali ad hoc legacy con IPC tipizzato.

Passi:

1. Definire namespace nuovo (es. `org.fildem`).
2. Progettare interfacce separate per:
   - stato finestra attiva
   - top-level menus
   - menu tree
   - attivazione voce
   - HUD
   - window actions
3. Produrre XML introspection completo.
4. Standardizzare payload/proprieta' (app id, title, window id logico, items, action ids, enabled/toggle).
5. Eliminare dipendenza concettuale da `MyService`/`Echo*`.

Deliverable:

1. `org.fildem.*.xml` versionato.
2. Linee guida di compatibilita' API.

### Fase D - Nuova extension GNOME 48+ (ESM)

**Obiettivo:** nuova extension moderna, non porting diretto.

Passi:

1. Creare struttura extension pulita (stessa directory o nuova dedicata).
2. Implementare `extension.js` ESM con scope minimo:
   - lifecycle
   - panel UI
   - tracking finestra attiva
   - bridge col daemon
3. Introdurre moduli interni:
   - `panel-controller.js`
   - `window-tracker.js`
   - `daemon-client.js`
   - `window-actions.js`
4. Evitare moduli shell interni quando esiste API pubblica equivalente.
5. Dichiarare compatibilita' GNOME 48+ dopo verifica API/path reali.

Deliverable:

1. Extension avviabile e installabile in sessione test.

### Fase E - Core C/GObject/GIO

**Obiettivo:** sostituire completamente il core Python.

Passi:

1. Creare `src/core/` con moduli piccoli:
   - `fildem-menu-registry`
   - `fildem-menu-model`
   - `fildem-menu-cache`
   - `fildem-dbus-client`
   - `fildem-dbus-server`
2. Usare `GDBusConnection`, `GDBusProxy`, `GDBusInterfaceSkeleton`.
3. Definire ownership/refcount/ciclo vita oggetti GObject.
4. Aggiungere serializzazione modello menu testabile.
5. Esporre API interne semplici e documentate.

Deliverable:

1. Libreria core compilabile.
2. API interna documentata.

### Fase F - HUD GTK4

**Obiettivo:** UI HUD nuova e disaccoppiata dal legacy GTK3.

Passi:

1. Creare `src/hud/` separato dal core.
2. Implementare HUD come `GtkApplication`.
3. Usare `GtkListView`/`GtkFilterListModel` per ricerca e navigazione.
4. Usare `GAction`/`GMenuModel` per attivazione.
5. Evitare porting diretto di widget GTK3 rimossi (`Gtk.Menu`, `Gtk.MenuBar`, `Gtk.AccelGroup`, popup grabs).

Deliverable:

1. HUD GTK4 funzionante con query/activate.

### Fase G - Shortcut moderni

**Obiettivo:** rimuovere dipendenza primaria da key grabbing X11.

Passi:

1. Implementare `shortcut-manager`.
2. Integrare `org.freedesktop.portal.GlobalShortcuts` come backend principale (dove disponibile).
3. Prevedere fallback controllati:
   - trigger dalla shell extension
   - backend X11 opzionale e separato
4. Documentare differenze capability Wayland/X11.

Deliverable:

1. Trigger HUD/menu funzionanti su Wayland dove supportati.

### Fase H - Test harness agentico

**Obiettivo:** validazione automatica di core, daemon e integrazione shell.

Passi:

1. Aggiungere unit test GLib per il core.
2. Aggiungere contract test D-Bus con fixture/mock publisher.
3. Introdurre fixture menu tree + golden output serializzati.
4. Preparare smoke test in sessione isolata con `dbus-run-session`.
5. Preparare test integrazione GNOME nested (separati da unit).
6. Usare attese su segnali/eventi, non sleep fissi.

Deliverable:

1. Pipeline test riproducibile locale/CI.

## Regola di migrazione dal legacy

Il codice legacy si usa solo per:

1. nomi/semantica menu
2. comportamento top-level panel
3. window actions supportate
4. flusso update su cambio finestra
5. aspettative utente

Non va copiato:

1. struttura Python attuale
2. `os.system`/thread launch legacy
3. workaround `GDK_BACKEND=x11`
4. widget GTK3 legacy
5. servizio D-Bus `Echo*`
6. coupling extension/HUD/backend

## Ordine di implementazione consigliato

1. Specifica funzionale legacy (Fase A)
2. Nuova API D-Bus (Fase C)
3. Skeleton daemon C (Fase E, minimo)
4. Nuova extension ESM (Fase D)
5. Menu model + top-level bridge (E + D)
6. HUD GTK4 (Fase F)
7. Shortcut manager (Fase G)
8. Test harness e hardening (Fase H)
