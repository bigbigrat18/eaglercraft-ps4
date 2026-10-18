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
//     hardware, so the screen's solid color doubles as the only
//     diagnostic signal available if something fails early. See the
//     COLOR_* constants below and the README's "Status colors" section.

#include <iostream>
#include <sstream>

#include <graphics.h>
#include "web_browser_dialog.h"

// log.h's DEBUGLOG macro expects this global to exist in the app (same
// convention every OpenOrbis sample that uses log.h follows).
std::stringstream debugLogStream;

namespace
{
	const char* kAssetPath = "/app0/assets/eaglercraft.html";

	constexpr int SCREEN_WIDTH = 1920;
	constexpr int SCREEN_HEIGHT = 1080;

	// Status colors -- see README "Status colors" for what each one means.
	const Color COLOR_BOOTING     = {64, 64, 64};
	const Color COLOR_NO_VIDEO    = {255, 255, 0};
	const Color COLOR_MODULE_FAIL = {255, 0, 0};
	const Color COLOR_INIT_FAIL   = {255, 140, 0};
	const Color COLOR_OPEN_FAIL   = {200, 0, 200};
	const Color COLOR_RUNNING     = {0, 200, 0};
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

	// Keeps presenting a solid status color every frame. If hasVideo is
	// false this is a no-op (nothing we can do without a video-out
	// handle), which itself would mean the screen stays black/whatever
	// the system left it at -- a distinct failure mode worth knowing too.
	auto presentStatus = [&]()
	{
		if(!hasVideo)
			return;
		scene->FrameBufferFill(statusColor);
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
				break;
			default:
				std::cerr << "Fatal: WebBrowserDialog/CommonDialog initialize failed." << std::endl;
				statusColor = COLOR_INIT_FAIL;
				break;
		}
		for(;;)
			presentStatus();
	}

	if(!dialog.OpenLocalFile(kAssetPath))
	{
		std::cerr << "Fatal: WebBrowserDialog open failed -- see "
			"source/web_browser_dialog.h confidence notes, the param "
			"struct layout is the most likely culprit." << std::endl;
		statusColor = COLOR_OPEN_FAIL;
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
			if(!dialog.OpenLocalFile(kAssetPath))
			{
				std::cerr << "Fatal: failed to reopen WebBrowserDialog." << std::endl;
				statusColor = COLOR_OPEN_FAIL;
				for(;;)
					presentStatus();
			}
		}
	}

	return 0;
}
