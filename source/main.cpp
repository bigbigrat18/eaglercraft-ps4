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
//     On an Open() failure specifically, the screen instead shows 8
//     colored bars encoding the actual sceWebBrowserDialogOpen() return
//     code in hex (see DrawHexCode below) -- a flat color can only tell
//     us *that* it failed, not *why*.

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
	const Color COLOR_MODULE_FAIL = {255, 0, 0};
	const Color COLOR_INIT_FAIL   = {255, 140, 0};
	const Color COLOR_RUNNING     = {0, 200, 0};

	// 16-entry palette for the hex-code bar display, chosen for maximum
	// visual distinctness in a phone photo of a TV screen -- see README
	// "Reading the hex-code bars" for the same table.
	const Color HEX_PALETTE[16] = {
		{0, 0, 0},       // 0 black
		{255, 255, 255}, // 1 white
		{255, 0, 0},     // 2 red
		{0, 255, 0},     // 3 green
		{0, 0, 255},     // 4 blue
		{255, 255, 0},   // 5 yellow
		{0, 255, 255},   // 6 cyan
		{255, 0, 255},   // 7 magenta
		{255, 140, 0},   // 8 orange
		{160, 0, 255},   // 9 purple
		{139, 69, 19},   // A brown
		{160, 160, 160}, // B gray
		{128, 0, 0},     // C maroon
		{0, 0, 128},     // D navy
		{128, 128, 0},   // E olive
		{0, 128, 128},   // F teal
	};

	// Draws `value`'s 8 hex nibbles (most significant first) as 8 equal
	// vertical bars across the full screen, one solid frame (no flipping
	// loop needed -- the caller re-presents it like any other status).
	void DrawHexCode(Scene2D* scene, uint32_t value)
	{
		int barWidth = SCREEN_WIDTH / 8;
		for(int i = 0; i < 8; i++)
		{
			int shift = (7 - i) * 4;
			int nibble = (value >> shift) & 0xF;
			scene->DrawRectangle(i * barWidth, 0, barWidth, SCREEN_HEIGHT, HEX_PALETTE[nibble]);
		}
	}
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
	bool showingHexCode = false;
	uint32_t hexCodeValue = 0;

	// Keeps presenting either a solid status color or the hex-code bars
	// every frame. If hasVideo is false this is a no-op (nothing we can
	// do without a video-out handle), which itself would mean the screen
	// stays black/whatever the system left it at -- a distinct failure
	// mode worth knowing too.
	auto presentStatus = [&]()
	{
		if(!hasVideo)
			return;
		if(showingHexCode)
			DrawHexCode(scene, hexCodeValue);
		else
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

	int32_t openResult = dialog.OpenLocalFile(kAssetPath);
	if(openResult < 0)
	{
		std::cerr << "Fatal: WebBrowserDialog open failed, rc=0x"
			<< std::hex << openResult << std::endl;
		showingHexCode = true;
		hexCodeValue = static_cast<uint32_t>(openResult);
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
				showingHexCode = true;
				hexCodeValue = static_cast<uint32_t>(reopenResult);
				for(;;)
					presentStatus();
			}
		}
	}

	return 0;
}
