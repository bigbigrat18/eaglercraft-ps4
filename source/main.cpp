// eaglercraft-ps4 -- kiosk-mode homebrew wrapper around EaglercraftX 1.8
//
// Boots straight into a fullscreen WKView pointed at a bundled, fully
// self-contained eaglercraft.html (no browser chrome, no URL bar). Input is
// a physical USB keyboard only -- no gamepad translation layer.
//
// GL/EGL context setup below is adapted near-verbatim from
// OpenOrbis-PS4-Toolchain/samples/piglet (credit: Specter / flat_z's PS4 GL
// test, via the OpenOrbis sample) -- that part is a known-working sequence.
// The WebKit2 integration in webkit_view.cpp/.h is NOT verified; see that
// file's header comment and the README before debugging a crash there.

#include <iostream>
#include <cstring>

#include <orbis/Pigletv2VSH.h> // pulls in EGL/GLES2 types too, see piglet sample
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>

#include "keyboard.h"
#include "webkit_view.h"

namespace
{
	constexpr int SCREEN_WIDTH = 1920;
	constexpr int SCREEN_HEIGHT = 1080;

	int32_t g_pigletModule = -1;
	int32_t g_precompiledShadersModule = -1;

	EGLDisplay g_display = EGL_NO_DISPLAY;
	EGLSurface g_surface = EGL_NO_SURFACE;
	EGLContext g_context = EGL_NO_CONTEXT;

	// --- verified Piglet bring-up, adapted from the OpenOrbis piglet sample ---

	bool LoadGraphicsModules()
	{
		std::string prefix = "/" + std::string(sceKernelGetFsSandboxRandomWord()) + "/common/lib/";
		int mstartRet = 0;

		g_pigletModule = static_cast<int32_t>(sceKernelLoadStartModule(
			(prefix + "libScePigletv2VSH.sprx").c_str(), 0, nullptr, 0, nullptr, &mstartRet));
		if(g_pigletModule < ORBIS_OK)
		{
			std::cerr << "Failed to load Piglet module, err=0x" << std::hex << g_pigletModule << std::endl;
			return false;
		}

		g_precompiledShadersModule = static_cast<int32_t>(sceKernelLoadStartModule(
			(prefix + "libScePrecompiledShaders.sprx").c_str(), 0, nullptr, 0, nullptr, &mstartRet));
		if(g_precompiledShadersModule < ORBIS_OK)
		{
			std::cerr << "Failed to load PrecompiledShaders module, err=0x" << std::hex << g_precompiledShadersModule << std::endl;
			return false;
		}

		return true;
	}

	bool CreateGLContext(int width, int height)
	{
		OrbisPglConfig pglConfig{};
		OrbisPglWindow renderWindow{ 0, static_cast<khronos_uint32_t>(width), static_cast<khronos_uint32_t>(height) };
		EGLConfig config{};
		EGLint numConfigs = 0;
		int major = -1, minor = -1;

		const EGLint attribs[] = {
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_DEPTH_SIZE, 0,
			EGL_STENCIL_SIZE, 0,
			EGL_SAMPLE_BUFFERS, 0,
			EGL_SAMPLES, 0,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_NONE,
		};
		const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		const EGLint windowAttribs[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE };

		std::memset(&pglConfig, 0, sizeof(pglConfig));
		pglConfig.size = sizeof(pglConfig);
		pglConfig.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
		pglConfig.processOrder = 1;
		pglConfig.systemSharedMemorySize = 250 * 1024 * 1024;
		pglConfig.videoSharedMemorySize = 512 * 1024 * 1024;
		pglConfig.maxMappedFlexibleMemory = 170 * 1024 * 1024;
		pglConfig.drawCommandBufferSize = 1 * 1024 * 1024;
		pglConfig.lcueResourceBufferSize = 1 * 1024 * 1024;
		pglConfig.dbgPosCmd_0x40 = width;
		pglConfig.dbgPosCmd_0x44 = height;
		pglConfig.dbgPosCmd_0x48 = 0;
		pglConfig.dbgPosCmd_0x4C = 0;
		pglConfig.unk_0x5C = 2;

		if(!scePigletSetConfigurationVSH(&pglConfig))
		{
			std::cerr << "scePigletSetConfigurationVSH failed." << std::endl;
			return false;
		}

		g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if(g_display == EGL_NO_DISPLAY) { std::cerr << "eglGetDisplay failed." << std::endl; return false; }

		if(!eglInitialize(g_display, &major, &minor)) { std::cerr << "eglInitialize failed." << std::endl; return false; }
		if(!eglBindAPI(EGL_OPENGL_ES_API)) { std::cerr << "eglBindAPI failed." << std::endl; return false; }
		if(!eglSwapInterval(g_display, 0)) { std::cerr << "eglSwapInterval failed." << std::endl; return false; }

		if(!eglChooseConfig(g_display, attribs, &config, 1, &numConfigs) || numConfigs < 1)
		{
			std::cerr << "eglChooseConfig failed." << std::endl;
			return false;
		}

		g_surface = eglCreateWindowSurface(g_display, config, &renderWindow, windowAttribs);
		if(g_surface == EGL_NO_SURFACE) { std::cerr << "eglCreateWindowSurface failed." << std::endl; return false; }

		g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, ctxAttribs);
		if(g_context == EGL_NO_CONTEXT) { std::cerr << "eglCreateContext failed." << std::endl; return false; }

		if(!eglMakeCurrent(g_display, g_surface, g_surface, g_context))
		{
			std::cerr << "eglMakeCurrent failed." << std::endl;
			return false;
		}

		return true;
	}

	// --- keyboard -> WKView event bridge (see webkit_view.h confidence note) ---

	uint16_t g_prevKeys[8];
	int g_prevKeyCount = 0;

	bool WasKeyDown(uint16_t code)
	{
		for(int i = 0; i < g_prevKeyCount; i++)
			if(g_prevKeys[i] == code)
				return true;
		return false;
	}

	uint32_t CurrentModifiers(Keyboard& kb)
	{
		uint32_t mods = 0;
		if(kb.CheckModifierKey(Keyboard::SHIFT)) mods |= ORBIS_WEBKEY_MOD_SHIFT;
		if(kb.CheckModifierKey(Keyboard::CTRL))  mods |= ORBIS_WEBKEY_MOD_CTRL;
		if(kb.CheckModifierKey(Keyboard::ALT))   mods |= ORBIS_WEBKEY_MOD_ALT;
		if(kb.CheckModifierKey(Keyboard::META))  mods |= ORBIS_WEBKEY_MOD_META;
		return mods;
	}

	// Polls the keyboard, diffs against last frame, and forwards key-down /
	// key-up / char events into the webview. Keeping the diff here (rather
	// than in Keyboard, which only reports current-frame state) is our own
	// logic and is independent of the WKView signature guesses.
	void PumpKeyboard(Keyboard& kb, WebkitView& web)
	{
		uint32_t mods = CurrentModifiers(kb);
		uint16_t curKeys[8];
		int curCount = 0;

		for(uint16_t code : kb.Keys())
		{
			if(curCount < 8)
				curKeys[curCount++] = code;

			if(!WasKeyDown(code))
			{
				OrbisWebKeyboardEventGuess ev{};
				ev.type = ORBIS_WEBKEY_DOWN;
				ev.virtualKeyCode = code;
				ev.modifiers = mods;
				web.HandleKeyboardEvent(ev);

				int ch = kb.Key2Char(code);
				if(ch > 0 && ch < 0x10000)
				{
					OrbisWebKeyboardEventGuess chEv{};
					chEv.type = ORBIS_WEBKEY_CHAR;
					chEv.virtualKeyCode = code;
					chEv.modifiers = mods;
					chEv.text[0] = static_cast<uint16_t>(ch);
					chEv.text[1] = 0;
					web.HandleKeyboardEvent(chEv);
				}
			}
		}

		for(int i = 0; i < g_prevKeyCount; i++)
		{
			bool stillDown = false;
			for(int j = 0; j < curCount; j++)
				if(curKeys[j] == g_prevKeys[i]) { stillDown = true; break; }

			if(!stillDown)
			{
				OrbisWebKeyboardEventGuess ev{};
				ev.type = ORBIS_WEBKEY_UP;
				ev.virtualKeyCode = g_prevKeys[i];
				ev.modifiers = mods;
				web.HandleKeyboardEvent(ev);
			}
		}

		std::memcpy(g_prevKeys, curKeys, sizeof(uint16_t) * curCount);
		g_prevKeyCount = curCount;
	}
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	std::cout << "eaglercraft-ps4 starting..." << std::endl;

	if(!LoadGraphicsModules())
	{
		std::cerr << "Fatal: graphics module load failed." << std::endl;
		for(;;);
	}

	if(!CreateGLContext(SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		std::cerr << "Fatal: GL context creation failed." << std::endl;
		for(;;);
	}

	// Hide the PS4 boot splash once we're ready to draw our own first frame.
	sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	sceSystemServiceHideSplashScreen();

	Keyboard keyboard;
	if(!keyboard.Init(-1))
	{
		std::cerr << "Fatal: keyboard init failed." << std::endl;
		for(;;);
	}

	WebkitView webview;
	if(!webview.Init(SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		std::cerr << "Fatal: WebKit2 view init failed -- see webkit_view.h/.cpp "
			"confidence notes, this is the least-verified part of the app." << std::endl;
		for(;;);
	}

	// Asset is bundled into the pkg at assets/eaglercraft.html, which lands
	// at /app0/assets/eaglercraft.html at runtime (app0 is the pkg's own
	// mount point, per OpenOrbis convention -- see the keyboard sample's
	// font-loading path for the same "/app0/..." pattern).
	webview.LoadLocalFile("/app0/assets/eaglercraft.html");

	for(;;)
	{
		PumpKeyboard(keyboard, webview);

		webview.PaintToCurrentGLContext();

		if(!eglSwapBuffers(g_display, g_surface))
		{
			std::cerr << "eglSwapBuffers failed: 0x" << std::hex << eglGetError() << std::endl;
			break;
		}
	}

	return 0;
}
