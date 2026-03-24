import Adw from 'gi://Adw';
import Gio from 'gi://Gio';
import Gtk from 'gi://Gtk';

import {ExtensionPreferences} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

const VISIBILITY_LABELS = [
    'Automatic',
    'Always visible',
    'Only on hover',
];

export default class FildemShellPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings('org.fildem.shell');

        const page = new Adw.PreferencesPage();
        const group = new Adw.PreferencesGroup({
            title: 'Panel',
            description: 'Top-level menu behavior in the GNOME panel.',
        });

        const visibilityModel = Gtk.StringList.new(VISIBILITY_LABELS);
        const visibilityRow = new Adw.ComboRow({
            title: 'Visibility mode',
            model: visibilityModel,
            selected: settings.get_int('visibility-mode'),
        });
        visibilityRow.connect('notify::selected', row => {
            settings.set_int('visibility-mode', row.selected);
        });

        const showLabelRow = new Adw.SwitchRow({
            title: 'Show app label when menu is visible',
            active: settings.get_boolean('show-app-label-when-menu-visible'),
        });
        settings.bind('show-app-label-when-menu-visible',
            showLabelRow, 'active', Gio.SettingsBindFlags.DEFAULT);

        const paddingAdjustment = new Gtk.Adjustment({
            lower: 0,
            upper: 24,
            step_increment: 1,
            page_increment: 2,
            value: settings.get_int('button-padding'),
        });
        const paddingRow = new Adw.SpinRow({
            title: 'Button padding',
            adjustment: paddingAdjustment,
        });
        settings.bind('button-padding', paddingRow, 'value', Gio.SettingsBindFlags.DEFAULT);

        const initialShortcut = settings.get_strv('hud-shortcut')[0] ?? '';
        const shortcutRow = new Adw.EntryRow({
            title: 'HUD shortcut',
            text: initialShortcut,
        });
        shortcutRow.connect('notify::text', row => {
            const text = (row.text ?? '').trim();
            settings.set_strv('hud-shortcut', text.length > 0 ? [text] : []);
        });

        group.add(visibilityRow);
        group.add(showLabelRow);
        group.add(paddingRow);
        group.add(shortcutRow);
        page.add(group);
        window.add(page);
    }
}
