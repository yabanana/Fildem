Name:           fildem-rewrite
Version:        1.0.0
Release:        1%{?dist}
Summary:        Fildem rewrite for GNOME 48+
License:        GPL-3.0-only
URL:            https://github.com/gonzaarcr/Fildem
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(gtk4)
Requires:       gnome-shell

%description
Fildem rewrite delivers:
- fildemd session daemon
- fildemhud GTK4 HUD
- GNOME Shell extension fildem-shell@fildem.org

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSE.txt
%doc README.md
%{_bindir}/fildemd
%{_bindir}/fildemhud
%{_bindir}/fildemctl
%{_libdir}/libfildem-core.so
%{_datadir}/applications/org.fildem.hud.desktop
%{_datadir}/fildem/interfaces/*
%{_datadir}/gnome-shell/extensions/fildem-shell@fildem.org/*
%{_prefix}/lib/systemd/user/fildemd.service

%changelog
* Mon Mar 23 2026 Fildem Maintainers <maintainers@fildem.org> - 1.0.0-1
- Initial rewrite package skeleton
