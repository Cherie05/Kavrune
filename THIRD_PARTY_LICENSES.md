# Dependency notices

Original Kavrune source, documentation and artwork are licensed under the [MIT License](LICENSE), copyright 2026 Kavrune contributors. Third-party components retain their own licenses. This notice does not grant additional rights to those components.

The application dynamically links distribution-provided libraries and calls system services. System shared libraries and services are not bundled in the Debian package. Development archives under `.deps` are excluded from project source and release artifacts.

| Component | License | Purpose / upstream |
|---|---|---|
| GTK4 | LGPL-2.1-or-later | Native UI; https://www.gtk.org/ |
| libadwaita | LGPL-2.1-or-later | Linux desktop widgets; https://gitlab.gnome.org/GNOME/libadwaita |
| GLib / GObject / GIO | LGPL-2.1-or-later | Main loop, D-Bus and descriptor transfer; https://gitlab.gnome.org/GNOME/glib |
| OpenSSL 3 libcrypto | Apache-2.0 | EVP digests; https://openssl-library.org/source/license/ |
| libstdc++ / GCC runtime | GPL-3.0-or-later WITH GCC-exception-3.1 | C++ runtime; https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html |
| glibc | LGPL-2.1-or-later and component-specific terms | System C runtime; https://www.gnu.org/software/libc/ |
| UDisks2 | GPL-2.0-or-later (service; inspect installed package notices) | External system service via D-Bus; https://github.com/storaged-project/udisks |
| Polkit | LGPL-2.0-or-later (system service; inspect package notices) | External authorization; https://gitlab.freedesktop.org/polkit/polkit |

Formatting is delegated to system UDisks2 and locally installed dosfstools, exfatprogs and e2fsprogs. They are external programs, not copied or linked into the project. GTK's transitive graphics/text/accessibility dependencies also have their own licenses. Exact licenses and component exceptions must be checked against installed package copyright notices and upstream sources for every release.

Build tools (CMake, pkg-config, GCC, GLib resource compiler) are external tools. The original SVG uses no external raster assets or fonts. Stock symbolic icons come from the user's desktop icon theme and are not included as project source.

`scripts/sbom.py` records direct runtime package versions available on the build host. It labels unreviewed details rather than guessing them. This file and the inventory do not constitute a complete dependency-license audit. Distributors must inspect the exact libraries, packages and notices shipped with their artifacts and satisfy the applicable license requirements.

Contributors adding external code, artwork, fonts or other dependencies must record the source, version, license and any required notices here. The current original SVG icon contains no third-party raster assets or embedded fonts.
