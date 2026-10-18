// eaglercraft-ps4 -- kiosk-mode homebrew wrapper around EaglercraftX 1.8
//
// Opens the PS4's Common Dialog WebBrowserDialog pointed at a bundled,
// fully self-contained eaglercraft.html. Input is a physical USB keyboard
// only (no gamepad translation) -- the system itself handles keyboard
// input to the dialog while it's open/focused, so there's no manual event
// forwarding here. See source/web_browser_dialog.h for the confidence
// notes on this module's API surface.
//
// This app also owns a minimal Scene2D video-out surface (see graphics.h,
// a toolchain-provided helper), for two reasons:
//  1. A Common Dialog overlays on top of whatever the app is already
//     presenting -- an app with no active video output may have nothing
//     for the compositor to show the dialog over.
//  2. There is no debug console available when testing this on real
//     hardware, so the screen itself doubles as the only diagnostic
//     signal available if something fails early: a flat color for coarse
//     pass/fail state (see COLOR_* below and the README's "Status
//     colors" section), and actual on-screen text for the exact
//     sceWebBrowserDialogOpen() return code on failure -- an earlier
//     version tried encoding that code as colored bars, which turned out
//     to be too easy to misread from a photo; text isn't.

#include <iostream>
#include <sstream>
#include <cstdio>

#include <graphics.h>
#include "web_browser_dialog.h"

// log.h's DEBUGLOG macro expects this global to exist in the app (same
// convention every OpenOrbis sample that uses log.h follows).
std::stringstream debugLogStream;

namespace
{
	const char* kAssetPath = "/app0/assets/eaglercraft.html";
	const char* kFontPath = "/app0/assets/fonts/font.ttf";

	constexpr int SCREEN_WIDTH = 1920;
	constexpr int SCREEN_HEIGHT = 1080;

	// Status colors -- see README "Status colors" for what each one means.
	const Color COLOR_BOOTING     = {64, 64, 64};
	const Color COLOR_MODULE_FAIL = {255, 0, 0};
	const Color COLOR_INIT_FAIL   = {255, 140, 0};
	const Color COLOR_OPEN_FAIL   = {200, 0, 200};
	const Color COLOR_RUNNING     = {0, 200, 0};

	const Color BLACK = {0, 0, 0};
	const Color WHITE = {255, 255, 255};
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	std::cout << "eaglercraft-ps4 starting..." << std::endl;

	// Heap-allocated and never freed, deliberately: graphics.cpp (a
	// toolchain-provided helper, not ours) declares ~Scene2D() in its
	// header but never defines it, and this app never returns anyway, so
	// matching the toolchain's own samples (which all do the same `new`,
	// never `delete`) avoids forcing the linker to resolve a destructor
	// that doesn't exist.
	auto scene = new Scene2D(SCREEN_WIDTH, SCREEN_HEIGHT, 4);
	bool hasVideo = scene->Init(0xC000000, 2);
	int frameID = 0;
	Color statusColor = COLOR_BOOTING;

	FT_Face font;
	bool hasFont = hasVideo && scene->InitFont(&font, kFontPath, 36);

	// errorText is drawn over the status color when non-null -- used to
	// show the exact sceWebBrowserDialogOpen() return code, far more
	// reliably than trying to encode it visually.
	const char* errorText = nullptr;
	char errorTextBuf[128];

	// Keeps presenting the current status color (and errorText, if set)
	// every frame. If hasVideo is false this is a no-op (nothing we can
	// do without a video-out handle), which itself would mean the screen
	// stays black/whatever the system left it at -- a distinct failure
	// mode worth knowing too.
	auto presentStatus = [&]()
	{
		if(!hasVideo)
			return;
		scene->FrameBufferFill(statusColor);
		if(errorText && hasFont)
			scene->DrawText(const_cast<char*>(errorText), font, 40, 60, statusColor, WHITE);
		scene->SubmitFlip(frameID);
		scene->FrameWait(frameID);
		scene->FrameBufferSwap();
		frameID++;
	};

	if(!hasVideo)
	{
		std::cerr << "Fatal: Scene2D video-out init failed." << std::endl;
		for(;;);
	}

	presentStatus();

	WebBrowserDialog dialog;
	WebBrowserDialogInitResult initResult = dialog.Init();
	if(initResult != WebBrowserDialogInitResult::OK)
	{
		switch(initResult)
		{
			case WebBrowserDialogInitResult::MODULE_LOAD_FAILED:
				std::cerr << "Fatal: failed to load the WebBrowserDialog sysmodule." << std::endl;
				statusColor = COLOR_MODULE_FAIL;
				errorText = "sceSysmoduleLoadModule(WEB_BROWSER_DIALOG) failed";
				break;
			default:
				std::cerr << "Fatal: WebBrowserDialog/CommonDialog initialize failed." << std::endl;
				statusColor = COLOR_INIT_FAIL;
				errorText = "CommonDialog/WebBrowserDialog Initialize failed";
				break;
		}
		for(;;)
			presentStatus();
	}

	int32_t openResult = dialog.OpenLocalFile(kAssetPath);
	if(openResult < 0)
	{
		std::cerr << "Fatal: WebBrowserDialog open failed, rc=0x"
			<< std::hex << openResult << std::endl;
		statusColor = COLOR_OPEN_FAIL;
		std::snprintf(errorTextBuf, sizeof(errorTextBuf),
			"sceWebBrowserDialogOpen failed: rc=0x%08X (%d)",
			static_cast<unsigned int>(openResult), static_cast<int>(openResult));
		errorText = errorTextBuf;
		for(;;)
			presentStatus();
	}

	statusColor = COLOR_RUNNING;

	for(;;)
	{
		presentStatus();

		if(!dialog.Pump())
		{
			// The dialog is the entire "game" -- if it got dismissed,
			// just reopen it rather than leaving a blank screen.
			int32_t reopenResult = dialog.OpenLocalFile(kAssetPath);
			if(reopenResult < 0)
			{
				std::cerr << "Fatal: failed to reopen WebBrowserDialog, rc=0x"
					<< std::hex << reopenResult << std::endl;
				statusColor = COLOR_OPEN_FAIL;
				std::snprintf(errorTextBuf, sizeof(errorTextBuf),
					"sceWebBrowserDialogOpen (reopen) failed: rc=0x%08X (%d)",
					static_cast<unsigned int>(reopenResult), static_cast<int>(reopenResult));
				errorText = errorTextBuf;
				for(;;)
					presentStatus();
			}
		}
	}

	return 0;
}
