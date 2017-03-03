// test_himo.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "test_himo.h"

#include "../himo.h"
#include <string>

#define MAX_LOADSTRING 100

// このコード モジュールに含まれる関数の宣言を転送します:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	::DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)WndProc);
	return NULL;
}

typedef std::basic_string<_TCHAR> tstring;
// value bound to HWND of window control
static himo::BoundData<HWND, tstring> bound_string(_T(""));
static himo::BoundData<HWND, BOOL> bound_boolean(TRUE);
// command bound to HWND of button mainly
static himo::BoundCommand<HWND> command_append, command_toggle, command_clear, command_enable(false), command_disable(true);
// as a facade for window message handler procedure
static himo::Binder<HWND> binder;

static void init_bindings(HWND hWnd)
{
	auto invalidate_rect = [](HWND h) { ::InvalidateRect(h, NULL, FALSE); }; // InvalidateRect returns BOOL type
	binder.AttachInvalidator(std::bind(invalidate_rect, hWnd));

	// Bind string value to show as window text
	bound_string.AttachSetter(
		[](HWND h, tstring str) {
		::SetWindowText(h, str.c_str());
	});
	bound_string.AttachGetter(
		[](HWND h) {
		wchar_t buf[256];
		::GetWindowText(h, buf, sizeof(buf));
		return tstring(buf);
	});
	bound_string.AttachComparator(
		[](tstring a, tstring b) {
		return (a == b) ? 0 : 1;
	});
	bound_string.AttachValidator(
		[](tstring a) { if (a.size() > 8) a.resize(8); return a; }
	);
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_EDIT1));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_EDIT2));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_EDIT3));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_EDIT4));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_EDIT5));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_STATIC_1));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_STATIC_2));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_STATIC_3));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_STATIC_4));
	binder.Bind(&bound_string, ::GetDlgItem(hWnd, IDC_STATIC_5));

	// Bind booleaan value to enable bound controls
	bound_boolean.AttachGetter(&::IsWindowEnabled);
	bound_boolean.AttachSetter(&::EnableWindow);
	bound_boolean.AttachComparator([](BOOL a, BOOL b) { return a ^ b; });
	binder.Bind(&bound_boolean, ::GetDlgItem(hWnd, IDC_EDIT1));
	binder.Bind(&bound_boolean, ::GetDlgItem(hWnd, IDC_EDIT2));
	binder.Bind(&bound_boolean, ::GetDlgItem(hWnd, IDC_EDIT3));
	binder.Bind(&bound_boolean, ::GetDlgItem(hWnd, IDC_EDIT4));
	binder.Bind(&bound_boolean, ::GetDlgItem(hWnd, IDC_EDIT5));

	// lambda formula to cast standartd bool into BOOL
	auto func_enwin = [](HWND h, bool en) { ::EnableWindow(h, (BOOL)en); };
	// Asynchronous command
	command_append.AttachEnabler(func_enwin );
	command_append.AttachAction(
		[](HWND) { ::Sleep(1000); bound_string = (tstring)bound_string + _T("1"); },
		true /* async */
	);
	binder.Bind(&command_append, ::GetDlgItem(hWnd, IDC_BUTTON1));

	// Synchronous commands
	command_toggle.AttachEnabler(func_enwin );
	command_toggle.AttachAction(
		[](HWND) { bound_boolean = !bound_boolean; }
	);
	binder.Bind(&command_toggle, ::GetDlgItem(hWnd, IDC_BUTTON2));

	command_clear.AttachEnabler(func_enwin);
	command_clear.AttachAction(
		[](HWND) { bound_string = _T(""); }
	);
	binder.Bind(&command_clear, ::GetDlgItem(hWnd, IDC_BUTTON3));

	command_enable.AttachEnabler(func_enwin);
	command_enable.AttachAction(
		[](HWND) { 
		command_append.Enable(true); 
		command_toggle.Enable(true);
		command_clear.Enable(true);
		command_enable.Enable(false);
		command_disable.Enable(true);
	}
	);
	binder.Bind(&command_enable, ::GetDlgItem(hWnd, IDC_BUTTON4));

	command_disable.AttachEnabler(func_enwin);
	command_disable.AttachAction(
		[](HWND) { 
		command_append.Enable(false);
		command_toggle.Enable(false);
		command_clear.Enable(false);
		command_enable.Enable(true);
		command_disable.Enable(false);
	}
	);
	binder.Bind(&command_disable, ::GetDlgItem(hWnd, IDC_BUTTON5));
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
	case WM_INITDIALOG:
		init_bindings(hWnd);

		break;
    case WM_COMMAND:
	{
		UINT uNotifyCode = (UINT)HIWORD(wParam);
		int idCtrl = (int)LOWORD(wParam);
		HWND wndCtrl = (HWND)lParam;
		// Excecute some actions bound to some values or commands
		binder.OnCommand(wndCtrl);
		switch (idCtrl)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}
	case WM_NOTIFY:
	{
		int idCtrl = (int)wParam;
		LPNMHDR pnmh = (LPNMHDR)lParam;
		HWND hwndCtrl = ::GetDlgItem(hWnd, idCtrl);
		// Notify some changes to some values or commands
		binder.OnNotify(hwndCtrl);
		break;
	}
	case WM_PAINT:
	{
		binder.OnDraw();
		break;
	}
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

