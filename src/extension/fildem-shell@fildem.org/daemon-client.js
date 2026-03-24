import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const BUS_NAME = 'org.fildem';
const OBJECT_PATH = '/org/fildem/v1';

const IFACE_WINDOW = 'org.fildem.v1.Window';
const IFACE_TOP_LEVEL = 'org.fildem.v1.TopLevel';
const IFACE_ACTIVATION = 'org.fildem.v1.Activation';
const IFACE_WINDOW_ACTIONS = 'org.fildem.v1.WindowActions';
const IFACE_HUD = 'org.fildem.v1.Hud';

function _variantTypeForValue(value) {
    if (typeof value === 'string')
        return new GLib.Variant('s', value);
    if (typeof value === 'boolean')
        return new GLib.Variant('b', value);
    if (Number.isInteger(value))
        return new GLib.Variant('i', value);
    if (typeof value === 'number')
        return new GLib.Variant('d', value);
    return new GLib.Variant('s', String(value ?? ''));
}

function _dictToVariant(dict) {
    const packed = {};
    for (const [key, value] of Object.entries(dict))
        packed[key] = _variantTypeForValue(value);

    return new GLib.Variant('a{sv}', packed);
}

function _tuple(...values) {
    return GLib.Variant.new_tuple(values);
}

function _parseTopLevel(rawItems) {
    return rawItems.map(item => ({
        id: item.id ?? '',
        label: item.label ?? '',
        enabled: item.enabled ?? true,
        visible: item.visible ?? true,
        mnemonic: item.mnemonic ?? '',
    })).filter(item => item.id.length > 0 && item.label.length > 0);
}

export class DaemonClient {
    constructor(log) {
        this._log = log;
        this._topLevelChangedHandlers = [];
        this._windowActionsChangedHandlers = [];
        this._activationRequestedHandlers = [];

        this._windowProxy = this._newProxy(IFACE_WINDOW);
        this._topLevelProxy = this._newProxy(IFACE_TOP_LEVEL);
        this._activationProxy = this._newProxy(IFACE_ACTIVATION);
        this._windowActionsProxy = this._newProxy(IFACE_WINDOW_ACTIONS);
        this._hudProxy = this._newProxy(IFACE_HUD);

        this._topLevelSignalId = this._topLevelProxy.connectSignal(
            'TopLevelChanged',
            (_proxy, _sender, [windowUid, items]) => {
                const parsed = _parseTopLevel(items);
                for (const handler of this._topLevelChangedHandlers)
                    handler(windowUid, parsed);
            }
        );

        this._windowActionsSignalId = this._windowActionsProxy.connectSignal(
            'WindowActionsChanged',
            (_proxy, _sender, [windowUid, actions]) => {
                for (const handler of this._windowActionsChangedHandlers)
                    handler(windowUid, actions);
            }
        );

        this._activationSignalId = this._activationProxy.connectSignal(
            'ActivationRequested',
            (_proxy, _sender, [windowUid, targetId]) => {
                for (const handler of this._activationRequestedHandlers)
                    handler(windowUid, targetId);
            }
        );
    }

    destroy() {
        if (this._topLevelProxy && this._topLevelSignalId)
            this._topLevelProxy.disconnectSignal(this._topLevelSignalId);
        if (this._windowActionsProxy && this._windowActionsSignalId)
            this._windowActionsProxy.disconnectSignal(this._windowActionsSignalId);
        if (this._activationProxy && this._activationSignalId)
            this._activationProxy.disconnectSignal(this._activationSignalId);

        this._topLevelSignalId = 0;
        this._windowActionsSignalId = 0;
        this._activationSignalId = 0;

        this._windowProxy = null;
        this._topLevelProxy = null;
        this._activationProxy = null;
        this._windowActionsProxy = null;
        this._hudProxy = null;
    }

    onTopLevelChanged(handler) {
        this._topLevelChangedHandlers.push(handler);
    }

    onWindowActionsChanged(handler) {
        this._windowActionsChangedHandlers.push(handler);
    }

    onActivationRequested(handler) {
        this._activationRequestedHandlers.push(handler);
    }

    updateActiveWindow(context) {
        try {
            const contextVariant = _dictToVariant(context);
            this._windowProxy.call_sync(
                'UpdateActiveWindow',
                _tuple(contextVariant),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
        } catch (error) {
            this._log(`UpdateActiveWindow failed: ${error}`);
        }
    }

    getTopLevel(windowUid) {
        try {
            const reply = this._topLevelProxy.call_sync(
                'GetTopLevel',
                _tuple(new GLib.Variant('s', windowUid)),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
            const [items] = reply.deep_unpack();
            return _parseTopLevel(items);
        } catch (error) {
            this._log(`GetTopLevel failed: ${error}`);
            return [];
        }
    }

    activateTopLevel(windowUid, topLevelId) {
        try {
            this._activationProxy.call_sync(
                'ActivateTopLevel',
                _tuple(
                    new GLib.Variant('s', windowUid),
                    new GLib.Variant('s', topLevelId)
                ),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
        } catch (error) {
            this._log(`ActivateTopLevel failed: ${error}`);
        }
    }

    setWindowActions(windowUid, actions) {
        const normalized = Array.isArray(actions) ? actions.map(a => String(a)) : [];
        try {
            this._windowActionsProxy.call_sync(
                'SetWindowActions',
                _tuple(
                    new GLib.Variant('s', windowUid),
                    new GLib.Variant('as', normalized)
                ),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
        } catch (error) {
            this._log(`SetWindowActions failed: ${error}`);
        }
    }

    requestHud(windowUid) {
        try {
            this._hudProxy.call_sync(
                'RequestHud',
                _tuple(new GLib.Variant('s', windowUid ?? '')),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
            return true;
        } catch (error) {
            this._log(`RequestHud failed: ${error}`);
            return false;
        }
    }

    listWindowActions(windowUid) {
        try {
            const reply = this._windowActionsProxy.call_sync(
                'ListWindowActions',
                _tuple(new GLib.Variant('s', windowUid)),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
            const [actions] = reply.deep_unpack();
            return actions;
        } catch (error) {
            this._log(`ListWindowActions failed: ${error}`);
            return [];
        }
    }

    activateWindowAction(windowUid, actionId) {
        try {
            this._windowActionsProxy.call_sync(
                'ActivateWindowAction',
                _tuple(
                    new GLib.Variant('s', windowUid),
                    new GLib.Variant('s', actionId)
                ),
                Gio.DBusCallFlags.NONE,
                -1,
                null
            );
        } catch (error) {
            this._log(`ActivateWindowAction failed: ${error}`);
        }
    }

    _newProxy(interfaceName) {
        return Gio.DBusProxy.new_for_bus_sync(
            Gio.BusType.SESSION,
            Gio.DBusProxyFlags.NONE,
            null,
            BUS_NAME,
            OBJECT_PATH,
            interfaceName,
            null
        );
    }
}
