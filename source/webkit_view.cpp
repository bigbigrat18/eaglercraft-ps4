#include <string>
#include <cstring>
#include <orbis/libkernel.h>
#include "webkit_view.h"
#include "log.h"

namespace
{
	// -1 until loaded; mirrors the pattern used by the piglet sample for
	// its own sceKernelLoadStartModule() handles.
	int32_t g_webkit2ModuleHandle = -1;

	// BEST-EFFORT module filename. The symbol list in orbis/WebKit2.h
	// strongly implies a module literally named for "WebKit2", and the
	// piglet sample proves this exact sceKernelLoadStartModule() pattern
	// (sandbox-prefixed path under /common/lib/) is how PS4 homebrew loads
	// first-party .sprx modules that aren't exposed via the small
	// sceSysmoduleLoadModule() allowlist. The filename itself is NOT
	// confirmed -- if this fails to load, try dumping
	// /system/common/lib/*.sprx from a jailbroken console's filesystem to
	// find the real name (it may be split into separate UI-process /
	// Web-process modules).
	const char* kWebkit2ModuleName = "libSceWebKit2.sprx";

	bool LoadWebkit2Module()
	{
		if(g_webkit2ModuleHandle >= 0)
			return true;

		std::string prefix = "/" + std::string(sceKernelGetFsSandboxRandomWord()) + "/common/lib/";
		std::string path = prefix + kWebkit2ModuleName;

		int mstartRet = 0;
		g_webkit2ModuleHandle = static_cast<int32_t>(
			sceKernelLoadStartModule(path.c_str(), 0, nullptr, 0, nullptr, &mstartRet));

		if(g_webkit2ModuleHandle < 0)
		{
			DEBUGLOG << "[DEBUG] [ERROR] Failed to load " << kWebkit2ModuleName
				<< " (tried " << path << "), err=0x" << std::hex << g_webkit2ModuleHandle;
			return false;
		}

		DEBUGLOG << "[DEBUG] Loaded " << kWebkit2ModuleName << ", module_start()=" << mstartRet;
		return true;
	}
}

WebkitView::WebkitView()
	: context(nullptr), pageGroup(nullptr), view(nullptr), page(nullptr)
{
}

WebkitView::~WebkitView()
{
	// No explicit teardown calls -- the stub header doesn't expose
	// WKRelease()/CFRelease()-style symbols we could confirm, and this app
	// is expected to run until the system kills the process. If a real
	// teardown path is needed later, add WKRelease(context) etc. here once
	// confirmed against hardware.
}

bool WebkitView::Init(int width, int height)
{
	if(!LoadWebkit2Module())
		return false;

	context = WKContextCreate();
	if(!context)
	{
		DEBUGLOG << "[DEBUG] [ERROR] WKContextCreate() returned null";
		return false;
	}

	WKStringRef groupId = WKStringCreateWithUTF8CString("eaglercraft-ps4");
	pageGroup = WKPageGroupCreateWithIdentifier(groupId);
	if(!pageGroup)
	{
		DEBUGLOG << "[DEBUG] [ERROR] WKPageGroupCreateWithIdentifier() returned null";
		return false;
	}

	view = WKViewCreate(context, pageGroup);
	if(!view)
	{
		DEBUGLOG << "[DEBUG] [ERROR] WKViewCreate() returned null";
		return false;
	}

	WKViewSetSize(view, width, height);
	WKViewSetIsVisible(view, true);
	WKViewSetFocused(view, true);
	// No browser chrome of our own, and we don't want WebKit painting a
	// default white/checkerboard background behind the page before it
	// finishes loading -- kiosk mode wants the eaglercraft page itself to
	// be the only thing ever visible.
	WKViewSetDrawsBackground(view, true);

	page = WKViewGetPage(view);
	if(!page)
	{
		DEBUGLOG << "[DEBUG] [ERROR] WKViewGetPage() returned null";
		return false;
	}

	return true;
}

void WebkitView::LoadLocalFile(const char* app0RelativePath)
{
	std::string url = std::string("file://") + app0RelativePath;
	WKURLRef wkUrl = WKURLCreateWithUTF8CString(url.c_str());
	WKPageLoadURL(page, wkUrl);
}

void WebkitView::Resize(int width, int height)
{
	WKViewSetSize(view, width, height);
}

void WebkitView::SetFocused(bool focused)
{
	WKViewSetFocused(view, focused);
}

void WebkitView::HandleKeyboardEvent(const OrbisWebKeyboardEventGuess& event)
{
	WKViewHandleKeyboardEvent(view, &event);
}

void WebkitView::PaintToCurrentGLContext()
{
	WKViewPaintToCurrentGLContext(view);
}
