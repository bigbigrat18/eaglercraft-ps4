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
//   status enum are REAL, confirmed definitions from the toolchain's own
//   headers, exercised by the toolchain's real, working MsgDialog sample.
// - sceWebBrowserDialog*'s exact *symbol names* (Initialize, Open, Close,
//   UpdateStatus, GetStatus, GetResult, Navigate, SetZoom, GetEvent,
//   Terminate, ResetCookie, SetCookie, OpenForPredeterminedContent) are
//   REAL, confirmed via `nm -D lib/libSceWebBrowserDialog.so` against the
//   actual shipped library.
// - The *argument types* for those symbols are NOT yet reverse-engineered
//   anywhere in OpenOrbis -- WebBrowserDialog.h only ships bare,
//   argument-less NID stubs (`void sceWebBrowserDialogOpen();`).
//   OrbisWebBrowserDialogParam/Result below are reconstructed by following
//   the exact same baseParam + size + userId + reserved[] shape every
//   other real, confirmed Common Dialog in this toolchain uses
//   (OrbisMsgDialogParam, used verbatim as the structural template). This
//   is a well-grounded guess, not one invented from nothing -- but the
//   extra `url` field and its offset could still be wrong. If
//   sceWebBrowserDialogOpen() returns an error or crashes, this struct is
//   the first thing to re-check/adjust.

#include <orbis/CommonDialog.h>
#include <orbis/_types/common_dialog.h>
#include <stdint.h>
#include <stddef.h>

extern "C" {
	typedef int32_t OrbisUserServiceUserId;

	// Best-effort reconstruction -- see confidence note above.
	typedef struct OrbisWebBrowserDialogParam
	{
		OrbisCommonDialogBaseParam baseParam;
		size_t size;
		const char* url;
		OrbisUserServiceUserId userId;
		char reserved[40];
	} OrbisWebBrowserDialogParam;

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
	bool OpenLocalFile(const char* app0RelativePath);

	// Call once per frame. Returns false if the dialog isn't currently
	// open (e.g. the user dismissed it) -- the caller should decide
	// whether to reopen it.
	bool Pump();
};
