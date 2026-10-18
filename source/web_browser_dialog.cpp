#include <cstring>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include "web_browser_dialog.h"
#include "log.h"

WebBrowserDialog::WebBrowserDialog()
	: isOpen(false), userId(-1)
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

	// WebBrowserDialogParam has a real userId field (SharpProspero-confirmed,
	// see web_browser_dialog.h) -- pass the actual active user rather than
	// leaving it 0/uninitialized. Same sceUserServiceInitialize +
	// sceUserServiceGetInitialUser pattern as the OpenOrbis keyboard sample.
	OrbisUserServiceInitializeParams userParams;
	userParams.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
	sceUserServiceInitialize(&userParams);
	sceUserServiceGetInitialUser(&userId);

	return WebBrowserDialogInitResult::OK;
}

int32_t WebBrowserDialog::OpenUrl(const char* url)
{
	OrbisWebBrowserDialogParam param;
	std::memset(&param, 0, sizeof(param));

	// Real convention, confirmed from CommonDialog.h's own
	// _orbisCommonDialogBaseParamInit()/_orbisCommonDialogSetMagicNumber()
	// helpers: size = sizeof(baseParam), magic = MAGIC + address-of-param.
	param.baseParam.size = sizeof(param.baseParam);
	param.baseParam.magic = static_cast<uint32_t>(
		ORBIS_COMMON_DIALOG_MAGIC_NUMBER + reinterpret_cast<uint64_t>(&param));

	param.size = sizeof(param);
	param.mode = ORBIS_WEB_BROWSER_DIALOG_MODE_DEFAULT;
	param.userId = userId;
	param.url = url;

	int32_t rc = sceWebBrowserDialogOpen(&param);
	if(rc < 0)
	{
		DEBUGLOG << "[DEBUG] [ERROR] sceWebBrowserDialogOpen failed, rc=0x" << std::hex << rc;
		return rc;
	}

	isOpen = true;
	return rc;
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
