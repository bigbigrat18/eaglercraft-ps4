#pragma once
//
// Wrapper around PS4's Common Dialog WebBrowserDialog
// (libSceWebBrowserDialog.sprx, ORBIS_SYSMODULE_WEB_BROWSER_DIALOG = 0x00AB).
//
// This replaces an earlier, now-abandoned attempt at binding the raw
// WebKit2 WKView/WKPage C API directly -- that API has NO corresponding
// library anywhere in the real OpenOrbis toolchain (confirmed with
// `nm -D` across every lib/*.so; there is no libSceWebKit2.so). Common
// Dialog WebBrowserDialog is the only webview mechanism that actually
// exists as a linkable library for homebrew.
//
// Trade-off (confirmed, not a guess): every Sony Common Dialog always
// shows at least a minimal system-owned dismiss affordance/border -- there
// is no "zero chrome at all" mode. This was the accepted trade-off after
// discovering raw WebKit2 isn't available.
//
// CONFIDENCE NOTE:
// - sceCommonDialogInitialize/IsUsed, OrbisCommonDialogBaseParam's layout
//   (size / reserved[36] / magic), the magic-number formula, and the
//   status enum are REAL, confirmed definitions from the OpenOrbis
//   toolchain's own headers, exercised by the toolchain's real, working
//   MsgDialog sample.
// - sceWebBrowserDialog*'s exact *symbol names* are REAL, confirmed via
//   `nm -D lib/libSceWebBrowserDialog.so` against the actual shipped
//   library.
// - OrbisWebBrowserDialogParam below is now taken directly from
//   SvenGDK/SharpProspero (github.com/SvenGDK/SharpProspero), a
//   PS4/PS5 interop SDK project whose WebBrowserDialog.cs documents this
//   struct field-by-field with explicit byte offsets. This replaces two
//   earlier guesses: a ~112-byte struct modeled on OrbisMsgDialogParam,
//   then a 64-byte struct inferred from an emulator project's test
//   buffer size that turned out to carry no real evidentiary weight
//   (that emulator's sceWebBrowserDialogOpen() treats its param as a
//   fully opaque, never-dereferenced pointer -- the 64-byte buffer in
//   its test was an arbitrary convenient size, not a confirmed struct
//   size). Both earlier guesses produced a real, confirmed
//   SCE_COMMON_DIALOG_ERROR_PARAM_INVALID (0x80B8000A) on hardware. The
//   real struct is 328 bytes and -- critically -- has a `mode` field
//   that SharpProspero's own docs say the service rejects if left at its
//   zero/Invalid value, which is exactly what both earlier guesses did
//   (they never set it, since neither guess even had the field). Still
//   not hardware-confirmed as of this comment, but this is a real,
//   offset-documented community source rather than another guess.

#include <orbis/CommonDialog.h>
#include <orbis/_types/common_dialog.h>
#include <stdint.h>
#include <stddef.h>

extern "C" {
	enum OrbisWebBrowserDialogMode
	{
		ORBIS_WEB_BROWSER_DIALOG_MODE_INVALID = 0, // rejected by the service -- must not be left as this
		ORBIS_WEB_BROWSER_DIALOG_MODE_DEFAULT = 1, // full standard presentation
		ORBIS_WEB_BROWSER_DIALOG_MODE_CUSTOM  = 2, // caller-sized/positioned window (width/height/parts fields apply)
	};

	// Matches SvenGDK/SharpProspero's WebBrowserDialogParam exactly,
	// field-for-field and offset-for-offset (see confidence note above).
	// Only mode/userId/url are populated for a Default-mode open; the
	// Custom-mode-only fields (width/height/position/parts/control) are
	// left zeroed, as are the optional callback/IME/WebView param
	// pointers.
	typedef struct OrbisWebBrowserDialogParam
	{
		OrbisCommonDialogBaseParam baseParam; // offset 0, 48 bytes
		size_t size;                          // offset 48
		int32_t mode;                         // offset 56 -- OrbisWebBrowserDialogMode
		int32_t userId;                       // offset 60
		const char* url;                      // offset 64
		void* callbackInitParam;              // offset 72 -- optional
		uint16_t width;                       // offset 80 -- Custom mode only
		uint16_t height;                      // offset 82 -- Custom mode only
		uint16_t positionX;                   // offset 84 -- Custom mode only
		uint16_t positionY;                   // offset 86 -- Custom mode only
		uint32_t parts;                       // offset 88 -- Custom mode only
		uint16_t headerWidth;                 // offset 92 -- Custom mode only
		uint16_t headerPositionX;             // offset 94 -- Custom mode only
		uint16_t headerPositionY;             // offset 96 -- Custom mode only
		uint16_t _padding0;                   // offset 98
		uint32_t control;                     // offset 100 -- Custom mode only
		void* imeParam;                       // offset 104 -- optional
		void* webViewParam;                   // offset 112 -- optional
		uint32_t animation;                   // offset 120
		uint8_t reserved[202];                // offset 124
		uint16_t _tailPadding;                // offset 326
	} OrbisWebBrowserDialogParam;             // total: 328 bytes

	static_assert(sizeof(OrbisWebBrowserDialogParam) == 328,
		"OrbisWebBrowserDialogParam must stay 328 bytes to match "
		"SharpProspero's documented WebBrowserDialogParam layout");

	typedef struct OrbisWebBrowserDialogResult
	{
		int32_t result;
		char reserved[32];
	} OrbisWebBrowserDialogResult;

	// Symbol names confirmed real (see note above). Argument types follow
	// the Common Dialog family convention.
	int32_t sceWebBrowserDialogInitialize(void);
	int32_t sceWebBrowserDialogTerminate(void);
	int32_t sceWebBrowserDialogOpen(const OrbisWebBrowserDialogParam* param);
	int32_t sceWebBrowserDialogNavigate(const char* url);
	int32_t sceWebBrowserDialogClose(void);
	OrbisCommonDialogStatus sceWebBrowserDialogUpdateStatus(void);
	OrbisCommonDialogStatus sceWebBrowserDialogGetStatus(void);
	int32_t sceWebBrowserDialogGetResult(OrbisWebBrowserDialogResult* result);
}

enum class WebBrowserDialogInitResult
{
	OK,
	MODULE_LOAD_FAILED,        // sceSysmoduleLoadModule
	COMMON_DIALOG_INIT_FAILED, // sceCommonDialogInitialize
	WEBBROWSER_INIT_FAILED,    // sceWebBrowserDialogInitialize
};

class WebBrowserDialog
{
	bool isOpen;
	int32_t userId;
public:
	WebBrowserDialog();

	// Loads libSceWebBrowserDialog.sprx, then sceCommonDialogInitialize()
	// + sceWebBrowserDialogInitialize(). Split into a 3-way result (rather
	// than plain bool) specifically so a caller with no debug console can
	// still tell which of the three calls failed, e.g. by showing a
	// different screen color per case.
	WebBrowserDialogInitResult Init();

	// Opens the dialog pointed at a file:// URL built from a local
	// pkg-relative path, e.g. "/app0/assets/eaglercraft.html". Real
	// keyboard input while the dialog is open/focused is handled by the
	// system itself (PS4 natively supports USB keyboards for its own
	// browser/text-entry UI) -- no manual input forwarding needed.
	//
	// Returns the raw sceWebBrowserDialogOpen() return code (>= 0 on
	// success, a negative SCE error code on failure) rather than a plain
	// bool -- with no debug console available for testing, the caller
	// displays this value directly (see main.cpp's on-screen error text)
	// since it's the only way to see what Sony's API is actually
	// rejecting.
	int32_t OpenLocalFile(const char* app0RelativePath);

	// Call once per frame. Returns false if the dialog isn't currently
	// open (e.g. the user dismissed it) -- the caller should decide
	// whether to reopen it.
	bool Pump();
};
