import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const KEY_HUD_SHORTCUT = 'hud-shortcut';

export class ShortcutManager {
    constructor(settings, onHudRequested, log) {
        this._settings = settings;
        this._onHudRequested = onHudRequested;
        this._log = log;
        this._settingsSignalId = 0;
    }

    enable() {
        this._bindKey();
        this._settingsSignalId = this._settings.connect(
            `changed::${KEY_HUD_SHORTCUT}`,
            () => this._bindKey()
        );
    }

    destroy() {
        if (this._settingsSignalId) {
            this._settings.disconnect(this._settingsSignalId);
            this._settingsSignalId = 0;
        }
        this._unbindKey();
    }

    _bindKey() {
        this._unbindKey();
        try {
            Main.wm.addKeybinding(
                KEY_HUD_SHORTCUT,
                this._settings,
                Meta.KeyBindingFlags.NONE,
                Shell.ActionMode.ALL,
                () => this._onHudRequested()
            );
        } catch (error) {
            this._log(`cannot bind HUD shortcut: ${error}`);
        }
    }

    _unbindKey() {
        try {
            Main.wm.removeKeybinding(KEY_HUD_SHORTCUT);
        } catch (_error) {
            /* no-op if not yet bound */
        }
    }
}
