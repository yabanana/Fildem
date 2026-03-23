# Fildem D-Bus API v1 (`org.fildem`)

Object path root: `/org/fildem/v1`  
Bus name: `org.fildem`

## `org.fildem.v1.Window`

1. `UpdateActiveWindow(a{sv} context)`
2. `GetActiveWindow() -> a{sv} context`
3. Signal `ActiveWindowChanged(a{sv} context)`

Required context keys:

1. `window_uid` (`s`)
2. `app_id` (`s`)

Optional context keys used by provider adapters:

1. `title` (`s`)
2. `workspace` (`i`)
3. `monitor` (`i`)
4. `backend` (`s`) values: `wayland` or `x11`
5. `xid` (`s` or integer-compatible variant)
6. `gtk_unique_bus_name` (`s`)
7. `gtk_window_object_path` (`s`)
8. `gtk_application_object_path` (`s`)
9. `gtk_menubar_object_path` (`s`)
10. `gtk_app_menu_object_path` (`s`)

## `org.fildem.v1.TopLevel`

1. `SetTopLevel(s window_uid, aa{sv} items)`
2. `GetTopLevel(s window_uid) -> aa{sv} items`
3. Signal `TopLevelChanged(s window_uid, aa{sv} items)`

Required item keys:

1. `id` (`s`)
2. `label` (`s`)

## `org.fildem.v1.MenuTree`

1. `SetMenuTree(s window_uid, aa{sv} nodes)`
2. `GetMenuTree(s window_uid) -> aa{sv} nodes`
3. `InvalidateMenuTree(s window_uid)`
4. Signal `MenuTreeChanged(s window_uid)`

Required node keys:

1. `id` (`s`)
2. `label` (`s`)

## `org.fildem.v1.Activation`

1. `ActivateTopLevel(s window_uid, s top_level_id)`
2. `ActivateMenuItem(s window_uid, s item_id)`
3. Signal `ActivationRequested(s window_uid, s target_id)`

Notes:

1. `target_id` can be a menu node id or `action:<window-action-id>`.
2. Window actions are executed shell-side by the extension when it receives `ActivationRequested`.

## `org.fildem.v1.WindowActions`

1. `SetWindowActions(s window_uid, as actions)`
2. `ListWindowActions(s window_uid) -> as actions`
3. `ActivateWindowAction(s window_uid, s action_id)`
4. Signal `WindowActionsChanged(s window_uid, as actions)`

Notes:

1. `actions` are stable action labels/ids exposed by the extension (shell-native actions).
2. `ActivateWindowAction` emits `ActivationRequested(window_uid, "action:<id>")` on `org.fildem.v1.Activation`.

## `org.fildem.v1.Hud`

1. `RequestHud(s window_uid)`
2. `Query(s window_uid, s term) -> aa{sv} results`
3. `Execute(s window_uid, s entry_id)`
4. Signal `HudRequested(s window_uid)`

Notes:

1. `Query` results include both menu entries and window actions.
2. For window actions, result ids are prefixed as `action:<id>`.
