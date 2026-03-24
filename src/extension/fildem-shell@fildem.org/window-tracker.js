import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

export class WindowTrackerController {
    constructor() {
        this._shellTracker = Shell.WindowTracker.get_default();
        this._signals = [];
        this._callback = null;
    }

    start(callback) {
        this._callback = callback;
        this._signals.push(global.display.connect('notify::focus-window', () => this._emit()));
        this._emit();
    }

    destroy() {
        for (const signalId of this._signals)
            global.display.disconnect(signalId);
        this._signals = [];
        this._callback = null;
    }

    _emit() {
        if (!this._callback)
            return;
        const win = global.display.get_focus_window();
        this._callback(this._buildContext(win), win);
    }

    _buildContext(win) {
        if (!win) {
            return {
                has_window: false,
                window_uid: '',
                app_id: '',
                title: '',
                workspace: -1,
                monitor: -1,
                is_maximized: false,
                is_focused: false,
                backend: Meta.is_wayland_compositor() ? 'wayland' : 'x11',
                xid: '',
                gtk_unique_bus_name: '',
                gtk_application_object_path: '',
                gtk_window_object_path: '',
                gtk_menubar_object_path: '',
                gtk_app_menu_object_path: '',
            };
        }

        const app = this._shellTracker.get_window_app(win);
        const workspace = win.get_workspace();
        const fallbackAppId = win.get_wm_class_instance?.() ??
            win.get_wm_class?.() ??
            `window-${win.get_id()}`;

        const context = {
            has_window: true,
            window_uid: `${win.get_id()}`,
            app_id: app?.get_id() ?? fallbackAppId,
            title: win.get_title() ?? '',
            workspace: workspace ? workspace.index() : -1,
            monitor: win.get_monitor(),
            is_maximized: win.get_maximized() === Meta.MaximizeFlags.BOTH,
            is_focused: true,
            backend: Meta.is_wayland_compositor() ? 'wayland' : 'x11',
            xid: this._extractXid(win),
            gtk_unique_bus_name: '',
            gtk_application_object_path: '',
            gtk_window_object_path: '',
            gtk_menubar_object_path: '',
            gtk_app_menu_object_path: '',
        };

        for (const key of [
            'gtk_unique_bus_name',
            'gtk_application_object_path',
            'gtk_window_object_path',
            'gtk_menubar_object_path',
            'gtk_app_menu_object_path',
        ]) {
            const value = win[key];
            if (value !== null && value !== undefined)
                context[key] = String(value);
        }

        return context;
    }

    _extractXid(win) {
        try {
            const description = win.get_description?.() ?? '';
            const match = description.match(/0x[0-9a-fA-F]+/);
            if (match)
                return `${Number.parseInt(match[0], 16)}`;
        } catch (_error) {
            /* keep empty xid */
        }
        return '';
    }
}
