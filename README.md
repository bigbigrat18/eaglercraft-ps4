# eaglercraft-ps4

Kiosk-mode PS4 homebrew wrapper around [EaglercraftX 1.8](https://github.com/3kh0/eaglercraft-1.8) (the
browser port of Minecraft 1.8.8). Boots straight into a fullscreen web view
with no address bar, no tabs, no browser chrome of any kind. Input is a
physical USB keyboard only -- there is no gamepad-to-keyboard translation
layer, by design.

## How it works

- PS4 ships a system WebKit2 module (the same engine used by the system
  browser, trophy sync screens, etc.). This app loads it directly, creates
  one `WKView` sized to the full screen, and points it at a bundled,
  fully self-contained `eaglercraft.html` -- no network fetch needed to
  load the game itself.
- Rendering uses the Piglet GLES2/EGL system library (the same one every
  other PS4 homebrew GL demo uses) as the surface WebKit paints into every
  frame.
- `source/keyboard.cpp` polls the hardware keyboard each frame and diffs it
  against the previous frame to synthesize key-down/key-up/char events,
  which get forwarded into the web view.
- No `libScePad` / controller code exists anywhere in this project, per the
  request that drove this build.

## Bundled asset: JS build, not WASM-GC

Two precompiled client builds were provided (`eaglercraft-js.html` and
`eaglercraft-wasm.html`). The JS build is fully self-contained (~34MB,
single file, assets embedded as base64) and only touches the network for
two *optional* things:

- `wss://relay.eaglercraft.net/` -- the Shared Worlds / LAN-world relay.
  Only connects if you actually start a shared world; harmless to leave in
  for an offline kiosk.
- `https://live.eaglercraft.org/api/event` -- looks like a usage/analytics
  ping fired on load. Left in as-is since this is your asset and you didn't
  ask for it to be modified; if you want it stripped, say so and I'll patch
  that one call out of `assets/eaglercraft.html`.

The WASM-GC build (`eaglercraft-wasm.html`) is a 114-line bootstrap loader
that fetches the real runtime and assets over the network at startup --
**not viable for an offline pkg**, so it was not used. If you want a
WASM-GC build bundled instead, you'd need the actual runtime/asset files it
loads, not just the bootstrap HTML.

## Known risk areas -- read before reporting a "broken" build

This sandbox has **no OpenOrbis toolchain binaries** (the release zip with
`PkgTool.Core`/`create-fself`/`create-gp4` and the prebuilt stub libraries
isn't reachable from here -- GitHub's asset CDN is blocked by this
container's network policy). Everything below was written against the
*public* OpenOrbis headers (cloned from source) and cross-checked against
real, working sample code in that repo, but **none of it has been compiled
or linked even once.** You will hit real compiler/linker errors on the
first build -- that's expected, send them back and they'll get fixed fast.

Confidence, component by component:

| Component | Confidence | Why |
|---|---|---|
| Keyboard polling (`source/keyboard.h/.cpp`) | **High** | Copied near-verbatim from `OpenOrbis-PS4-Toolchain/samples/keyboard`, a real working sample. Only the `log.h` include path changed. |
| EGL/Piglet GL context (`source/main.cpp`) | **High** | Adapted near-verbatim from `samples/piglet/piglet/PigletApplication.cpp`, a real working sample (credit: Specter / flat_z's `ps4_gl_test`). |
| Module loading pattern (`sceKernelLoadStartModule` + sandbox-prefixed path) | **High** | Same proven pattern piglet uses to load `libScePigletv2VSH.sprx`. |
| `libSceWebKit2.sprx` as the module filename | **Low** | Inferred from the header name `WebKit2.h`; real filename is unconfirmed and may be split into separate processes. |
| `WKContextCreate`, `WKPageGroupCreateWithIdentifier`, `WKStringCreateWithUTF8CString`, `WKURLCreateWithUTF8CString`, `WKPageLoadURL` | **Medium** | `include/orbis/WebKit2.h` only lists bare symbol names (no arg types), but these names and shapes come from the real, long-public WebKit2 C API that this module is derived from, so the signatures are a well-grounded reconstruction, not a guess from nothing. |
| `WKViewCreate(context, pageGroup)` 2-arg signature | **Low** | Platform views are the one part of the WebKit2 C API every port defines differently; the real PS4 signature may take more/different args. |
| `WKViewSetSize`/`SetIsVisible`/`SetFocused`/`SetDrawsBackground` exact signatures | **Low-Medium** | Names confirmed present in the stub header; exact arg types guessed as plain `int`/`bool`. |
| `WKViewPaintToCurrentGLContext` | **Medium** | Name and "paint into whatever GL context is current" behavior is a reasonable read of the symbol name combined with the fact WebKit2's own symbol list includes `WKViewPaint` *and* a GL-context variant; arg list guessed as view-only. |
| Keyboard event struct passed to `WKViewHandleKeyboardEvent` (`OrbisWebKeyboardEventGuess`) | **Low** | This struct is **invented** by me, shaped after WebKit's internal `NativeWebKeyboardEvent` concept. There is currently no evidence for its real layout on PS4. This is the single most likely thing to need rework. |

If `WKViewCreate`/`WKViewHandleKeyboardEvent` turn out wrong, the most
useful next step is usually finding how an existing PS4 title or homebrew
project that already uses the system browser calls these (a kernel dump +
disassembly of `libSceWebKit2.sprx`'s real export table would settle the
struct layouts directly). Send me whatever you find and I'll fix the
bindings in `source/webkit_view.h/.cpp` -- everything WebKit-specific is
isolated there on purpose so a corrected signature is a one-file change.

## Building

On a machine with the OpenOrbis PS4 Toolchain installed and `OO_PS4_TOOLCHAIN`
set:

```sh
make
```

This produces `IV0000-BREW00200_00-EAGLERCRAFTPS400.pkg` in the repo root,
ready to install on a jailbroken PS4. `make clean` removes build output.

If the stub-lib generator didn't produce a `libSceWebKit2.a` under
`$OO_PS4_TOOLCHAIN/lib` (see "Known risk areas" above), the link step will
fail with undefined references for the `WK*` symbols -- that means the
toolchain's symbol database doesn't have `WebKit2.h` wired up to a stub lib
yet and it needs generating via `orbis-lib-gen` first. Let me know if you
hit that and I'll walk through it.

## Project layout

```
eaglercraft-ps4/
├── Makefile              # OpenOrbis build + pkg packaging
├── source/
│   ├── main.cpp           # entry point, EGL/Piglet setup, main loop
│   ├── keyboard.h/.cpp     # hardware keyboard polling (from OpenOrbis sample)
│   └── webkit_view.h/.cpp  # WKView wrapper -- see confidence table above
├── assets/
│   └── eaglercraft.html   # bundled, self-contained EaglercraftX 1.8 client (JS build)
└── sce_sys/
    ├── icon0.png           # placeholder icon (from OpenOrbis sample assets)
    └── about/right.sprx    # boilerplate rights file (same one every OpenOrbis sample ships)
```

## Not included (by request)

- No gamepad/`libScePad` input anywhere -- keyboard only.
- No modification to the eaglercraft client's relay/analytics endpoints
  (see "Bundled asset" above) unless you ask.
