# Accessibility and keyboard use

Kavrune uses native GTK/libadwaita controls and follows the desktop's theme, font and animation preferences. USB selectors and checksum/format fields have persistent text labels and accessibility relationships. Text status accompanies color, and critical results remain on screen. This is implemented accessibility support, not a completed screen-reader or contrast certification.

| Action | Keyboard or control |
|---|---|
| Switch tasks | Ctrl+1 Create, Ctrl+2 Format, Ctrl+3 Backup, Ctrl+4 Check download, Ctrl+5 Check USB, Ctrl+6 Activity, Ctrl+7 How to use |
| Move through controls | Native Tab/Shift+Tab; Space/Enter activates the focused control |
| Select a USB or option | Native dropdown keyboard navigation; no target is silently selected |
| Read legal/support documents | How to use → Legal & privacy, or About → Terms, privacy and disclaimer |
| Search a legal document | Ctrl+F, type text; Enter/arrows move between matches; Escape clears search or closes |
| Work in a small window | Use the header navigation button to open tasks; pages scroll vertically |
| Reduce motion | Disable animations in the desktop accessibility settings; the app stops custom pulses/spinners/transitions while elapsed progress text continues |

Automatic suspend/logout is deferred during active jobs where the desktop honors the request. It is not a guarantee against manual/forced shutdown or power loss. The app must remain open until safe cancellation or completion; storage operations cannot be undone.

Automated actual-widget checks verify labels/relationships, task states, legal search, reduced motion and close/export behavior. Private light/dark and compact/default visual checks exist. Complete Orca traversal, keyboard usability with real users, high-contrast measurements, 200% text/scaling, alternative fonts and supported-language/RTL acceptance remain pending. File chooser and authorization behavior also depend on the desktop/portal and require manual testing.

Report an accessibility issue through [SUPPORT.md](../SUPPORT.md), naming the application version, Linux desktop, assistive tool, scale/theme, affected task and expected keyboard behavior. Avoid sharing personal paths or drive serials. See [TEST_PLAN.md](../TEST_PLAN.md) for release acceptance.
