import GObject from 'gi://GObject';
import Clutter from 'gi://Clutter';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';

const VISIBILITY_AUTO = 0;
const VISIBILITY_ALWAYS = 1;
const VISIBILITY_HOVER = 2;

const FildemIndicator = GObject.registerClass(
class FildemIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.0, 'FildemShellIndicator');

        this._menusVisible = false;
        this._menuButtons = [];

        this._rootBox = new St.BoxLayout({
            style_class: 'panel-status-menu-box',
            reactive: true,
        });
        this._appLabel = new St.Label({
            text: '',
            y_align: Clutter.ActorAlign.CENTER,
            style_class: 'fildem-app-label',
        });
        this._menusBox = new St.BoxLayout({
            style_class: 'fildem-top-level-box',
            reactive: true,
        });

        this._rootBox.add_child(this._appLabel);
        this._rootBox.add_child(this._menusBox);
        this.add_child(this._rootBox);
    }

    setButtonPadding(padding) {
        for (const button of this._menuButtons)
            button.set_style(`padding-left: ${padding}px; padding-right: ${padding}px;`);
    }

    setAppLabel(label) {
        this._appLabel.set_text(label ?? '');
    }

    setAppLabelVisible(visible) {
        this._appLabel.visible = visible;
    }

    setTopLevelItems(items, onActivate) {
        this._clearTopLevelItems();

        for (const item of items) {
            const button = new St.Button({
                can_focus: true,
                reactive: true,
                track_hover: true,
                style_class: 'panel-button fildem-top-level-item',
                label: item.label,
            });

            button.connect('clicked', () => {
                onActivate(item.id, item.label);
            });
            this._menusBox.add_child(button);
            this._menuButtons.push(button);
        }

        this.setMenusVisible(this._menusVisible);
    }

    setWindowActions(actions, onActivate) {
        this.menu.removeAll();

        if (actions.length === 0)
            return;

        const section = new PopupMenu.PopupMenuSection();
        for (const actionId of actions) {
            const item = new PopupMenu.PopupMenuItem(actionId);
            item.connect('activate', () => onActivate(actionId));
            section.addMenuItem(item);
        }
        this.menu.addMenuItem(section);
    }

    setMenusVisible(visible) {
        this._menusVisible = visible;
        this._menusBox.visible = visible;
    }

    _clearTopLevelItems() {
        for (const button of this._menuButtons)
            button.destroy();
        this._menuButtons = [];
    }
});

export class PanelController {
    constructor(settings, callbacks) {
        this._settings = settings;
        this._callbacks = callbacks;

        this._context = null;
        this._isHover = false;
        this._panelSignalIds = [];
        this._settingsSignalIds = [];

        this._indicator = new FildemIndicator();
        Main.panel.addToStatusArea('fildem-shell-indicator', this._indicator, 0, 'left');

        this._panelSignalIds.push(Main.panel.connect('enter-event', () => {
            this._isHover = true;
            this._refreshVisibility();
        }));
        this._panelSignalIds.push(Main.panel.connect('leave-event', () => {
            this._isHover = false;
            this._refreshVisibility();
        }));

        this._settingsSignalIds.push(this._settings.connect('changed::visibility-mode', () => this._refreshVisibility()));
        this._settingsSignalIds.push(this._settings.connect('changed::show-app-label-when-menu-visible', () => this._refreshVisibility()));
        this._settingsSignalIds.push(this._settings.connect('changed::button-padding', () => this._refreshPadding()));

        this._refreshPadding();
        this._refreshVisibility();
    }

    destroy() {
        for (const id of this._panelSignalIds)
            Main.panel.disconnect(id);
        this._panelSignalIds = [];

        for (const id of this._settingsSignalIds)
            this._settings.disconnect(id);
        this._settingsSignalIds = [];

        this._indicator.destroy();
    }

    setContext(context) {
        this._context = context;
        this._indicator.setAppLabel(context.app_id || context.title || '');
        this._refreshVisibility();
    }

    setTopLevelItems(items) {
        this._indicator.setTopLevelItems(items, (id, label) => {
            this._callbacks.onTopLevelActivate(id, label);
        });
        this._refreshPadding();
        this._refreshVisibility();
    }

    setWindowActions(actions) {
        this._indicator.setWindowActions(actions, actionId => {
            this._callbacks.onWindowActionActivate(actionId);
        });
    }

    _refreshPadding() {
        this._indicator.setButtonPadding(this._settings.get_int('button-padding'));
    }

    _refreshVisibility() {
        const mode = this._settings.get_int('visibility-mode');
        const context = this._context ?? {};
        const hasWindow = Boolean(context.has_window);
        const isMaximized = Boolean(context.is_maximized);

        let showMenus = false;
        if (!hasWindow) {
            showMenus = false;
        } else if (mode === VISIBILITY_ALWAYS) {
            showMenus = true;
        } else if (mode === VISIBILITY_HOVER) {
            showMenus = this._isHover;
        } else if (mode === VISIBILITY_AUTO) {
            showMenus = isMaximized || this._isHover;
        }

        const showAppLabelWhenMenuVisible = this._settings.get_boolean('show-app-label-when-menu-visible');

        this._indicator.setMenusVisible(showMenus);
        this._indicator.setAppLabelVisible(!showMenus || showAppLabelWhenMenuVisible);
    }
}
