import GLib from 'gi://GLib';
import Meta from 'gi://Meta';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as WindowMenu from 'resource:///org/gnome/shell/ui/windowMenu.js';

const TOGGLED_SUFFIX = ' (On)';

function _stripToggleSuffix(actionId) {
    if (actionId.endsWith(TOGGLED_SUFFIX))
        return actionId.slice(0, -TOGGLED_SUFFIX.length);
    return actionId;
}

function _withToggleSuffixIfEnabled(label, enabled) {
    return enabled ? `${label}${TOGGLED_SUFFIX}` : label;
}

export class WindowActionsController {
    constructor(daemonClient, log) {
        this._daemonClient = daemonClient;
        this._log = log;
        this._window = null;
    }

    setWindow(window) {
        this._window = window ?? null;
    }

    list() {
        const win = this._window;
        if (!win)
            return [];

        const actions = [];
        const type = win.get_window_type();

        if (win.can_minimize())
            actions.push('Minimize');

        if (win.can_maximize())
            actions.push(win.get_maximized() ? 'Unmaximize' : 'Maximize');

        if (win.allows_move())
            actions.push('Move');

        if (win.allows_resize())
            actions.push('Resize');

        if (win.titlebar_is_onscreen() &&
            type !== Meta.WindowType.DOCK &&
            type !== Meta.WindowType.DESKTOP) {
            actions.push('Move Titlebar Onscreen');
        }

        if (win.get_maximized() === Meta.MaximizeFlags.BOTH ||
            type === Meta.WindowType.DOCK ||
            type === Meta.WindowType.DESKTOP ||
            type === Meta.WindowType.SPLASHSCREEN) {
            actions.push(_withToggleSuffixIfEnabled('Always on Top', win.is_above()));
        }

        if (Main.sessionMode.hasWorkspaces &&
            (!Meta.prefs_get_workspaces_only_on_primary() || win.is_on_primary_monitor())) {
            const isSticky = win.is_on_all_workspaces();
            if (win.is_always_on_all_workspaces()) {
                actions.push(_withToggleSuffixIfEnabled('Always on Visible Workspace', isSticky));
            }

            if (!isSticky) {
                const workspace = win.get_workspace();
                if (workspace && workspace !== workspace.get_neighbor(Meta.MotionDirection.LEFT))
                    actions.push('Move to Workspace Left');
                if (workspace && workspace !== workspace.get_neighbor(Meta.MotionDirection.RIGHT))
                    actions.push('Move to Workspace Right');
                if (workspace && workspace !== workspace.get_neighbor(Meta.MotionDirection.UP))
                    actions.push('Move to Workspace Up');
                if (workspace && workspace !== workspace.get_neighbor(Meta.MotionDirection.DOWN))
                    actions.push('Move to Workspace Down');
            }
        }

        const display = global.display;
        const monitorIndex = win.get_monitor();
        if (display.get_n_monitors() > 1 && monitorIndex >= 0) {
            if (display.get_monitor_neighbor_index(monitorIndex, Meta.DisplayDirection.UP) !== -1)
                actions.push('Move to Monitor Up');
            if (display.get_monitor_neighbor_index(monitorIndex, Meta.DisplayDirection.DOWN) !== -1)
                actions.push('Move to Monitor Down');
            if (display.get_monitor_neighbor_index(monitorIndex, Meta.DisplayDirection.LEFT) !== -1)
                actions.push('Move to Monitor Left');
            if (display.get_monitor_neighbor_index(monitorIndex, Meta.DisplayDirection.RIGHT) !== -1)
                actions.push('Move to Monitor Right');
        }

        if (win.can_close())
            actions.push('Close');

        return actions;
    }

    sync(windowUid) {
        if (!windowUid)
            return [];

        const actions = this.list();
        this._daemonClient.setWindowActions(windowUid, actions);
        return actions;
    }

    requestActivation(windowUid, actionId) {
        if (!windowUid || !actionId)
            return;
        this._daemonClient.activateWindowAction(windowUid, actionId);
    }

    execute(actionId) {
        const win = this._window;
        const action = _stripToggleSuffix(actionId ?? '');
        if (!win || !action)
            return;

        switch (action) {
        case 'Minimize':
            win.minimize();
            break;
        case 'Unmaximize':
            win.unmaximize(Meta.MaximizeFlags.BOTH);
            break;
        case 'Maximize':
            win.maximize(Meta.MaximizeFlags.BOTH);
            break;
        case 'Move':
            this._grabOperation(win, Meta.GrabOp.KEYBOARD_MOVING);
            break;
        case 'Resize':
            this._grabOperation(win, Meta.GrabOp.KEYBOARD_RESIZING_UNKNOWN);
            break;
        case 'Move Titlebar Onscreen':
            win.shove_titlebar_onscreen();
            break;
        case 'Always on Top':
            if (win.is_above())
                win.unmake_above();
            else
                win.make_above();
            break;
        case 'Always on Visible Workspace':
            if (win.is_on_all_workspaces())
                win.unstick();
            else
                win.stick();
            break;
        case 'Move to Workspace Left':
            this._moveToWorkspace(win, Meta.MotionDirection.LEFT);
            break;
        case 'Move to Workspace Right':
            this._moveToWorkspace(win, Meta.MotionDirection.RIGHT);
            break;
        case 'Move to Workspace Up':
            this._moveToWorkspace(win, Meta.MotionDirection.UP);
            break;
        case 'Move to Workspace Down':
            this._moveToWorkspace(win, Meta.MotionDirection.DOWN);
            break;
        case 'Move to Monitor Up':
            this._moveToMonitor(win, Meta.DisplayDirection.UP);
            break;
        case 'Move to Monitor Down':
            this._moveToMonitor(win, Meta.DisplayDirection.DOWN);
            break;
        case 'Move to Monitor Left':
            this._moveToMonitor(win, Meta.DisplayDirection.LEFT);
            break;
        case 'Move to Monitor Right':
            this._moveToMonitor(win, Meta.DisplayDirection.RIGHT);
            break;
        case 'Close':
            win.delete(global.get_current_time());
            break;
        default:
            this._log(`Unsupported window action: ${action}`);
            break;
        }
    }

    _grabOperation(win, grabOp) {
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 100, () => {
            try {
                WindowMenu.WindowMenu.prototype._grabAction(
                    win,
                    grabOp,
                    global.display.get_current_time_roundtrip()
                );
            } catch (error) {
                this._log(`window grab action failed: ${error}`);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _moveToWorkspace(win, direction) {
        const workspace = win.get_workspace();
        if (!workspace)
            return;
        win.change_workspace(workspace.get_neighbor(direction));
    }

    _moveToMonitor(win, direction) {
        const monitorIndex = win.get_monitor();
        const newIndex = global.display.get_monitor_neighbor_index(monitorIndex, direction);
        if (newIndex !== -1)
            win.move_to_monitor(newIndex);
    }
}
