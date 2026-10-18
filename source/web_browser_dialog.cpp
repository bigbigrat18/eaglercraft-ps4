#include <string>
#include <cstring>
#include <orbis/Sysmodule.h>
#include "web_browser_dialog.h"
#include "log.h"

WebBrowserDialog::WebBrowserDialog()
	: isOpen(false)
{
}

WebBrowserDialogInitResult WebBrowserDialog::Init()
{
	if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_WEB_BROWSER_DIALOG) < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] Failed to load ORBIS_SYSMODULE_WEB_BROWSER_DIALOG";
		return WebBrowserDialogInitResult::MODULE_LOAD_FAILED;
	}

	if(sceCommonDialogInitialize() < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] sceCommonDialogInitialize failed";
		return WebBrowserDialogInitResult::COMMON_DIALOG_INIT_FAILED;
	}

	if(sceWebBrowserDialogInitialize() < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] sceWebBrowserDialogInitialize failed";
		return WebBrowserDialogInitResult::WEBBROWSER_INIT_FAILED;
	}

	return WebBrowserDialogInitResult::OK;
}

bool WebBrowserDialog::OpenLocalFile(const char* app0RelativePath)
{
	std::string url = std::string("file://") + app0RelativePath;

	OrbisWebBrowserDialogParam param;
	std::memset(&param, 0, sizeof(param));

	// Real convention, confirmed from CommonDialog.h's own
	// _orbisCommonDialogBaseParamInit()/_orbisCommonDialogSetMagicNumber()
	// helpers: size = sizeof(baseParam), magic = MAGIC + address-of-param.
	param.baseParam.size = sizeof(param.baseParam);
	param.baseParam.magic = static_cast<uint32_t>(
		ORBIS_COMMON_DIALOG_MAGIC_NUMBER + reinterpret_cast<uint64_t>(&param));

	param.size = sizeof(param);
	param.url = url.c_str();
	param.userId = -1; // -1: whichever user is the initial/active one

	if(sceWebBrowserDialogOpen(&param) < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] sceWebBrowserDialogOpen failed";
		return false;
	}

	isOpen = true;
	return true;
}

bool WebBrowserDialog::Pump()
{
	if(!isOpen)
		return false;

	OrbisCommonDialogStatus status = sceWebBrowserDialogUpdateStatus();

	if(status == ORBIS_COMMON_DIALOG_STATUS_FINISHED)
	{
		// The dialog is meant to be the entire "game" here -- if it
		// finished (user dismissed it, or an internal error), close it out
		// and let the caller decide whether to reopen.
		DEBUGLOG << "[DEBUG] WebBrowserDialog finished/dismissed";
		sceWebBrowserDialogClose();
		isOpen = false;
		return false;
	}

	return true;
}
