# eaglercraft-ps4

Kiosk-mode PS4 homebrew wrapper around [EaglercraftX 1.8](https://github.com/3kh0/eaglercraft-1.8) (the
browser port of Minecraft 1.8.8). Opens a fullscreen web view with no
address bar, no tabs, and no browser chrome beyond what the system itself
always adds to its dialogs (see "Known trade-off" below). Input is a
physical USB keyboard, handled entirely by the system -- no gamepad code
anywhere in this project.

## How it works

PS4 does **not** expose the raw WebKit2 engine (`WKView`/`WKPage`) to
homebrew -- that was the first approach tried here, and it turned out to be
a dead end: there is no `libSceWebKit2.so` anywhere in the real OpenOrbis
toolchain's library set. What *is* real and linkable is Sony's **Common
Dialog WebBrowserDialog** (`libSceWebBrowserDialog.sprx`,
`ORBIS_SYSMODULE_WEB_BROWSER_DIALOG = 0x00AB`) -- the same dialog family
every PS4 game uses for message boxes, save-data prompts, etc., just the
webview variant of it.

- `source/web_browser_dialog.h/.cpp` loads that module and opens it
  pointed at a `file://` URL for the bundled, self-contained
  `eaglercraft.html`.
- The system handles rendering (it's a compositor overlay, not something
  the app draws into -- no EGL/GL context needed anywhere in this app)
  and real keyboard input (PS4 natively supports USB keyboards for its own
  browser/text UI) on its own. There's no manual input-forwarding code.
- `source/main.cpp` just opens the dialog and polls its status in a loop,
  reopening it if it ever gets dismissed, since the dialog *is* the app.

## Known trade-off: can't fully remove system chrome

Every Sony Common Dialog -- message boxes, save prompts, this webview --
always shows at least a minimal system-owned dismiss affordance/border.
There's no "zero chrome" mode available to homebrew; raw WebKit2 access
(which would allow that) simply isn't linkable. This was confirmed, not
assumed, and the trade-off was accepted before building on this approach.
It's still far closer to "no browser UI" than any real browser (no URL
bar, no tabs, no navigation controls) -- just not literally invisible.

## Bundled asset: JS build, not WASM-GC

Two precompiled client builds were provided (`eaglercraft-js.html` and
`eaglercraft-wasm.html`). The JS build is fully self-contained (~34MB,
single file, assets embedded as base64) and only touches the network for
two *optional* things:

- `wss://relay.eaglercraft.net/` -- the Shared Worlds / LAN-world relay.
  Only connects if you actually start a shared world; harmless to leave in
  for an offline kiosk.
- `https://live.eaglercraft.org/api/event` -- looks like a usage/analytics
  ping fired on load. Left in as-is since this is your asset and you
  didn't ask for it to be modified; say so if you want it stripped.

The WASM-GC build is a 114-line bootstrap loader that fetches the real
runtime and assets over the network at startup -- not viable for an
offline pkg, so it wasn't used.

## This has actually been built and packaged

Unlike the first draft of this project, this is no longer untested
guesswork about toolchain plumbing: the real OpenOrbis toolchain release
(`toolchain-llvm-18.tar.gz`, v0.5.4) was downloaded and used directly in
this sandbox to compile, link, and package this exact project, producing a
real `.pkg`. That caught (and fixed) a genuine bug: every OpenOrbis sample
Makefile's `$(wildcard assets/**/*)` pattern silently matches nothing
under plain GNU Make (`**` isn't a `wildcard` feature) -- the asset was
being left out of the pkg entirely. Fixed here with `$(shell find assets
-type f)`.

**Update after a real hardware test:** the first built pkg installed but
failed to launch with PS4's generic "the data for the application is
corrupted" dialog -- before any of this project's own code ran. Root
cause: `sce_module/` was empty. The toolchain's *git source* only ships
`.gitkeep` placeholders there (no binaries belong in git), so building
from a plain `git clone` never surfaces this -- but the real release
package shows every single one of the toolchain's samples, no exceptions,
bundles `sce_module/libc.prx` and `sce_module/libSceFios2.prx` alongside
the eboot. A homebrew ELF linked against the toolchain's libc stub needs
that matching `.prx` bundled in the pkg itself, or the loader's version
check fails and the system reports it as generic data corruption. Fixed
by bundling both files; this repo now carries them.

What's still *not* verified is anything that only matters once the pkg is
actually running as the signed PS4 process it's not being run as here --
i.e. whether the `OrbisWebBrowserDialogParam` struct this project invented
matches Sony's real one closely enough for `sceWebBrowserDialogOpen()` to
actually work.

Confidence, component by component:

| Component | Confidence | Why |
|---|---|---|
| Compiles, links, and packages into a valid pkg | **Confirmed** | Actually done, in this sandbox, against the real toolchain release -- not inferred. |
| `ORBIS_SYSMODULE_WEB_BROWSER_DIALOG = 0x00AB`, loading via `sceSysmoduleLoadModule` | **Confirmed** | Found explicitly in the toolchain's own `_types/sysmodule.h`, comment included (`// libSceWebBrowserDialog.sprx`). |
| `sceWebBrowserDialog*` symbol names (Open, Close, UpdateStatus, GetStatus, GetResult, Navigate, SetZoom, GetEvent, Initialize, Terminate, ResetCookie, SetCookie, OpenForPredeterminedContent) | **Confirmed** | Read directly off the real shipped `lib/libSceWebBrowserDialog.so` with `nm -D`. |
| `OrbisCommonDialogBaseParam` layout (`size`/`reserved[36]`/`magic`) and the magic-number formula | **Confirmed** | Real struct from the toolchain's own headers, exercised by its working `MsgDialog` sample, which was built and run through the same process here (header-only, not hardware-testable, but the struct itself is Sony's real one, not a guess). |
| `OrbisWebBrowserDialogParam`'s extra fields (`url`, `userId`) and offsets | **Best-effort guess** | WebBrowserDialog hasn't been reverse-engineered down to argument types anywhere in OpenOrbis yet. This struct follows the exact same shape every *other*, fully-reversed Common Dialog in this toolchain uses (`OrbisMsgDialogParam` as the template) -- a well-grounded guess, not one invented from nothing, but still the single most likely thing to need correcting on real hardware. |
| Whether `file://` URLs are even permitted by this dialog, vs. needing `sceWebBrowserDialogOpenForPredeterminedContent` or a cert-whitelisted scheme | **Unverified** | Can't tell from headers alone; if `Open()` succeeds but the page never loads, this is the next thing to check. |

If `sceWebBrowserDialogOpen()` returns an error or the dialog opens but
never loads the page, the param struct above is the first thing to
revisit -- send back whatever you observe (return codes, anything printed
via the dialog's own UI) and the struct will get corrected.

## Building

On a machine with the OpenOrbis PS4 Toolchain installed and `OO_PS4_TOOLCHAIN`
set:

```sh
make
```

This produces `IV0000-BREW00200_00-EAGLERCRAFTPS400.pkg` in the repo root,
ready to install on a jailbroken PS4. `make clean` removes build output.

(Two dependency issues showed up only because this sandbox is an unusual,
minimal Ubuntu 24.04 container -- a normal OpenOrbis dev machine shouldn't
hit either: the bundled `PkgTool.Core` is a .NET app that needed
`DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` for missing ICU data, and needed
`libssl1.1` installed since 24.04 only ships `libssl3`. Mentioned here only
in case your setup is similarly unusual.)

## Project layout

```
eaglercraft-ps4/
├── Makefile                      # OpenOrbis build + pkg packaging
├── source/
│   ├── main.cpp                   # entry point, dialog open/pump loop
│   └── web_browser_dialog.h/.cpp  # WebBrowserDialog wrapper -- see confidence table above
├── assets/
│   └── eaglercraft.html           # bundled, self-contained EaglercraftX 1.8 client (JS build)
└── sce_sys/
    ├── icon0.png                  # placeholder icon (from OpenOrbis sample assets)
    └── about/right.sprx           # boilerplate rights file (same one every OpenOrbis sample ships)
```

## Not included (by request)

- No gamepad/`libScePad` input anywhere -- keyboard only, and the system
  handles it natively for this dialog.
- No modification to the eaglercraft client's relay/analytics endpoints
  (see "Bundled asset" above) unless you ask.
