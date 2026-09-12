// Elevation.h : running the service manager with administrator rights
//
// The manifest keeps srvman at asInvoker (see src/srvman.manifest). Everything the list
// shows is readable through SC_MANAGER_ENUMERATE_SERVICE, which any user has, so demanding
// a UAC prompt just to look at the services would be a tax on the common case. Only the
// operations that change something - start, stop, create, delete, and applying properties -
// need the administrators group, and Windows cannot add it to a token that already exists.
// The process has to be replaced by an elevated one.
//
// So each of those operations goes through CMainDlg::EnsureElevated() first, which
// relaunches srvman through ShellExecuteEx()'s "runas" verb and closes the current
// instance. The new one is handed /select:<service> so the selection survives the restart;
// nothing else does, and the action itself is deliberately not replayed - the user gets an
// elevated window with the same service selected and clicks the button again. Replaying it
// would mean starting or deleting a service on the strength of a click that happened in a
// process that no longer exists.
//
// Declining the UAC prompt is an answer, not a failure: the unelevated instance stays up
// and keeps working as a viewer.
//
#pragma once

namespace Elevation
{
	//! Releases the cached shield bitmap. Call once, on the way out.
	void Shutdown();

	//! True when the process token is a full administrator token.
	/*! False both for a filtered token under UAC and for a user who is not an
		administrator at all. The two cases need no telling apart here: "runas" shows the
		first one a consent prompt and the second one a credential prompt, and either way
		what comes back is an elevated process. */
	bool IsElevated();

	//! Puts the UAC shield on a push button. Needs comctl32 v6, which stdafx.h asks for.
	void SetButtonShield(HWND hwndButton);

	//! Puts the UAC shield in a menu item's bitmap slot, leaving its text alone.
	/*! The bitmap is created once and owned by this module - a menu does not copy it, so
		it has to outlive every menu it is set on, and Shutdown() is what frees it. */
	void SetMenuItemShield(HMENU hMenu, UINT nCommandID);

	//! Starts a second srvman with an elevated token and the given arguments.
	/*! \param lpArguments command line for the new instance, or NULL for none. Quote
	           anything that can contain a space; CommandLineToArgvW() unquotes it again.
		\returns true if the elevated instance was started, in which case the caller must
		         abandon what it was doing and close - two service managers editing the
		         same machine is not a state worth reasoning about. False means the user
		         dismissed the prompt or it could not be shown; nothing happened and the
		         caller carries on unelevated. */
	bool Relaunch(HWND hwndParent, LPCTSTR lpArguments);
}
