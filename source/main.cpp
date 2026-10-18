// eaglercraft-ps4 -- kiosk-mode homebrew wrapper around EaglercraftX 1.8
//
// Opens the PS4's Common Dialog WebBrowserDialog pointed at a bundled,
// fully self-contained eaglercraft.html. Input is a physical USB keyboard
// only (no gamepad translation) -- the system itself handles keyboard
// input to the dialog while it's open/focused, so there's no manual event
// forwarding here. See source/web_browser_dialog.h for the confidence
// notes on this module's API surface.

#include <iostream>
#include <sstream>

#include "web_browser_dialog.h"

// log.h's DEBUGLOG macro expects this global to exist in the app (same
// convention every OpenOrbis sample that uses log.h follows).
std::stringstream debugLogStream;

namespace
{
	const char* kAssetPath = "/app0/assets/eaglercraft.html";
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	std::cout << "eaglercraft-ps4 starting..." << std::endl;

	WebBrowserDialog dialog;
	if(!dialog.Init())
	{
		std::cerr << "Fatal: WebBrowserDialog init failed." << std::endl;
		for(;;);
	}

	if(!dialog.OpenLocalFile(kAssetPath))
	{
		std::cerr << "Fatal: WebBrowserDialog open failed -- see "
			"source/web_browser_dialog.h confidence notes, the param "
			"struct layout is the most likely culprit." << std::endl;
		for(;;);
	}

	for(;;)
	{
		if(!dialog.Pump())
		{
			// The dialog is the entire "game" -- if it got dismissed,
			// just reopen it rather than leaving a blank screen.
			if(!dialog.OpenLocalFile(kAssetPath))
			{
				std::cerr << "Fatal: failed to reopen WebBrowserDialog." << std::endl;
				for(;;);
			}
		}
	}

	return 0;
}
