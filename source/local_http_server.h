#pragma once
//
// Tiny loopback-only HTTP server for one purpose: WebBrowserDialog opened
// successfully (real struct fix confirmed that), but rejected a file://
// URL with "this data is not supported by the PS4" -- a content-level
// rejection, not an API error. SharpProspero's own docs/dialogs.md only
// ever shows this dialog opened against http(s):// addresses
// (WebBrowser.Open("https://example.com")), never file://, which lines
// up with that symptom: this dialog is almost certainly built for remote
// web content, not local files.
//
// Workaround: serve the bundled, self-contained eaglercraft.html over a
// plain TCP socket on 127.0.0.1, and point the dialog at that instead of
// a file:// path. Since the asset is one single, fully self-contained
// page (no separate asset requests), every request gets the exact same
// response -- no routing/parsing needed.
//
// Socket API (socket/bind/listen/accept, plain POSIX) and std::thread
// usage both verified against OpenOrbis-PS4-Toolchain's own samples
// (samples/networking and samples/threading respectively) -- no extra
// libs needed beyond what's already linked (confirmed: neither sample's
// Makefile links -lSceNet or -lpthread for this).

#include <cstdint>
#include <string>

class LocalHttpServer
{
	std::string fileContent;
	uint16_t port;

public:
	LocalHttpServer();

	// Reads the given app0-relative file into memory, binds a loopback
	// TCP listener on the given port, and spawns a detached background
	// thread that serves that file's content (as text/html) to every
	// request. Returns false on any failure (file read, socket, bind,
	// listen).
	bool Start(const char* app0RelativePath, uint16_t listenPort);

	uint16_t Port() const { return port; }
};
