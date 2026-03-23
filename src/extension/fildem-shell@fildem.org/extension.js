import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {DaemonClient} from './daemon-client.js';
import {PanelController} from './panel-controller.js';
import {WindowTrackerController} from './window-tracker.js';
import {WindowActionsController} from './window-actions.js';
import {ShortcutManager} from './shortcut-manager.js';

import GLib from 'gi://GLib';

function _log(message) {
    global.log(`[fildem-shell] ${message}`);
}

export default class FildemShellExtension extends Extension {
    enable() {
        this._settings = this.getSettings('org.fildem.shell');
        this._daemonClient = new DaemonClient(_log);
        this._windowTracker = new WindowTrackerController();
        this._windowActions = new WindowActionsController(this._daemonClient, _log);
        this._shortcutManager = new ShortcutManager(this._settings, () => this._launchHud(), _log);
        this._currentContext = null;
        this._currentWindow = null;

        this._panel = new PanelController(this._settings, {
            onTopLevelActivate: id => this._activateTopLevel(id),
            onWindowActionActivate: actionId => this._activateWindowAction(actionId),
        });

        this._daemonClient.onTopLevelChanged((windowUid, items) => {
            if (this._currentContext?.window_uid === windowUid)
                this._panel.setTopLevelItems(items);
        });

        this._daemonClient.onWindowActionsChanged((windowUid, actions) => {
            if (this._currentContext?.window_uid === windowUid)
                this._panel.setWindowActions(actions);
        });

        this._daemonClient.onActivationRequested((windowUid, targetId) => {
            this._onActivationRequested(windowUid, targetId);
        });

        this._windowTracker.start((context, window) => this._onWindowContextChanged(context, window));
        this._shortcutManager.enable();
    }

    disable() {
        this._currentContext = null;

        if (this._windowTracker) {
            this._windowTracker.destroy();
            this._windowTracker = null;
        }

        if (this._panel) {
            this._panel.destroy();
            this._panel = null;
        }

        if (this._daemonClient) {
            this._daemonClient.destroy();
            this._daemonClient = null;
        }

        if (this._shortcutManager) {
            this._shortcutManager.destroy();
            this._shortcutManager = null;
        }

        this._currentWindow = null;
        this._windowActions = null;
        this._settings = null;
    }

    _onWindowContextChanged(context, window) {
        this._currentContext = context;
        this._currentWindow = window;
        this._windowActions.setWindow(window);
        this._panel.setContext(context);

        if (!context.has_window || !context.window_uid) {
            this._panel.setTopLevelItems([]);
            this._panel.setWindowActions([]);
            return;
        }

        this._daemonClient.updateActiveWindow(context);
        const actions = this._windowActions.sync(context.window_uid);
        this._panel.setTopLevelItems(this._daemonClient.getTopLevel(context.window_uid));
        this._panel.setWindowActions(actions);
    }

    _activateTopLevel(topLevelId) {
        const windowUid = this._currentContext?.window_uid;
        if (!windowUid || !topLevelId)
            return;
        this._daemonClient.activateTopLevel(windowUid, topLevelId);
    }

    _activateWindowAction(actionId) {
        const windowUid = this._currentContext?.window_uid;
        if (!windowUid || !actionId)
            return;
        this._windowActions.requestActivation(windowUid, actionId);
    }

    _onActivationRequested(windowUid, targetId) {
        if (windowUid !== this._currentContext?.window_uid || !targetId)
            return;

        if (targetId.startsWith('action:')) {
            this._windowActions.execute(targetId.slice('action:'.length));
            const actions = this._windowActions.sync(windowUid);
            this._panel.setWindowActions(actions);
        }
    }

    _launchHud() {
        const windowUid = this._currentContext?.window_uid ?? '';
        this._daemonClient.requestHud(windowUid);
        try {
            GLib.spawn_command_line_async('fildemhud');
        } catch (error) {
            _log(`failed to launch HUD: ${error}`);
        }
    }
}
