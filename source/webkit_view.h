#pragma once
//
// WKView wrapper around the PS4 system WebKit2 module.
//
// ============================================================================
// CONFIDENCE NOTE -- READ BEFORE DEBUGGING A CRASH HERE
// ============================================================================
// include/orbis/WebKit2.h in the OpenOrbis toolchain only lists NID-matched
// *symbol names* with no argument/return types (every declaration there is
// literally `void SomeFunction();`) -- that header exists purely to produce a
// linkable stub library, not to describe the real calling convention. Sony's
// PS4 firmware implements the actual public "WebKit2 C API" that originated
// in the open-source WebKit project (the same WKPage/WKView/WKContext family
// used historically by WebKitGTK and other embedders), so the function NAMES
// below are taken straight from that header's symbol list and should be
// correct. The argument lists, struct layouts, and the exact system module
// filename are this file's best-effort reconstruction from the public
// WebKit2 C API shape -- they have NOT been compiled or run against real
// PS4 hardware, because this sandbox has no PS4 SDK/toolchain binaries to
// test against (see README "Known risk areas"). Treat every declaration
// below as a hypothesis to be confirmed on real hardware, not a known-good
// binding.
// ============================================================================

#include <stdint.h>

// ---- Opaque WebKit2 C API ref types -------------------------------------
// Mutable objects (Context/Page/PageGroup/View) use non-const pointers;
// value-ish objects (String/URL) use const pointers. This mirrors the
// const-ness convention of the real open-source WebKit2 C API.
extern "C" {
	typedef struct OpaqueWKContext*   WKContextRef;
	typedef struct OpaqueWKPageGroup* WKPageGroupRef;
	typedef struct OpaqueWKPage*      WKPageRef;
	typedef struct OpaqueWKView*      WKViewRef;
	typedef const struct OpaqueWKString* WKStringRef;
	typedef const struct OpaqueWKURL*    WKURLRef;

	// Core WebKit2 C API (name + arg shape assumed from the public API this
	// is derived from -- see confidence note above).
	WKContextRef   WKContextCreate(void);
	WKStringRef    WKStringCreateWithUTF8CString(const char* string);
	WKURLRef       WKURLCreateWithUTF8CString(const char* url);
	WKPageGroupRef WKPageGroupCreateWithIdentifier(WKStringRef identifier);

	// Platform (PS4) view -- GUESSED 2-arg constructor. If the real
	// signature takes additional args (e.g. a window/surface handle), this
	// will fail to link or will misbehave at runtime; report back the
	// compiler/linker error so the signature can be corrected.
	WKViewRef WKViewCreate(WKContextRef context, WKPageGroupRef pageGroup);
	WKPageRef WKViewGetPage(WKViewRef view);

	void WKViewSetSize(WKViewRef view, int width, int height);
	void WKViewSetIsVisible(WKViewRef view, bool isVisible);
	void WKViewSetFocused(WKViewRef view, bool isFocused);
	void WKViewSetDrawsBackground(WKViewRef view, bool drawsBackground);

	void WKPageLoadURL(WKPageRef page, WKURLRef url);

	// Renders WebKit's compositor output into whatever GL context/FBO is
	// currently bound (the caller -- main.cpp -- must eglMakeCurrent first).
	void WKViewPaintToCurrentGLContext(WKViewRef view);
}

// Our own guess at a flat keyboard event struct to hand to WKView, modeled
// after WebKit's internal NativeWebKeyboardEvent shape (type / virtual key /
// native scan code / modifier bitmask / UTF-16 text). This struct and the
// WKViewHandleKeyboardEvent entry point are UNVERIFIED -- there is no
// confirmation PS4's WKView even reads a struct shaped like this.
enum OrbisWebKeyEventType
{
	ORBIS_WEBKEY_DOWN = 0,
	ORBIS_WEBKEY_UP = 1,
	ORBIS_WEBKEY_CHAR = 2,
};

enum OrbisWebKeyModifier
{
	ORBIS_WEBKEY_MOD_SHIFT = 1 << 0,
	ORBIS_WEBKEY_MOD_CTRL  = 1 << 1,
	ORBIS_WEBKEY_MOD_ALT   = 1 << 2,
	ORBIS_WEBKEY_MOD_META  = 1 << 3,
};

struct OrbisWebKeyboardEventGuess
{
	int32_t  type;             // OrbisWebKeyEventType
	uint32_t virtualKeyCode;   // sceKeyboard keycode, passed through as-is
	uint32_t modifiers;        // OrbisWebKeyModifier bitmask
	uint16_t text[8];          // UTF-16, null-terminated; used for CHAR events
};

extern "C" {
	void WKViewHandleKeyboardEvent(WKViewRef view, const OrbisWebKeyboardEventGuess* event);
}

// High-level wrapper used by main.cpp. Keeps all of the above isolated in
// one place so a corrected signature only needs to change here.
class WebkitView
{
	WKContextRef   context;
	WKPageGroupRef pageGroup;
	WKViewRef      view;
	WKPageRef      page;

public:
	WebkitView();
	~WebkitView();

	// Loads libSceWebKit2.sprx (best-effort module name -- see .cpp) and
	// creates the context/pageGroup/view/page chain. Returns false on any
	// failure.
	bool Init(int width, int height);

	// Points the view at a file:// URL built from a local pkg-relative path,
	// e.g. "/app0/assets/eaglercraft.html".
	void LoadLocalFile(const char* app0RelativePath);

	void Resize(int width, int height);
	void SetFocused(bool focused);

	void HandleKeyboardEvent(const OrbisWebKeyboardEventGuess& event);

	// Call once per frame with the target GL context already current.
	void PaintToCurrentGLContext();
};
