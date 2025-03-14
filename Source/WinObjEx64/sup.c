/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2015 - 2022
*
*  TITLE:       SUP.C
*
*  VERSION:     1.93
*
*  DATE:        13 May 2022
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"
#include "treelist/treelist.h"
#include "extras/extrasSSDT.h"

//
// Setup info database.
//
SAPIDB g_sapiDB;

//
// SCM info database.
//
SCMDB g_scmDB;

//
// Types collection.
//
POBJECT_TYPES_INFORMATION g_pObjectTypesInfo = NULL;

HWND g_hwndBanner = NULL;

//#define _PROFILE_MEMORY_USAGE_


#ifdef _PROFILE_MEMORY_USAGE_
ULONG g_cHeapAlloc = 0;
#endif

int __cdecl supxHandlesLookupCallback(
    void const* first,
    void const* second);

int __cdecl supxHandlesLookupCallback2(
    void const* first,
    void const* second);

ULONG MmProtectToValue[32] = {
    PAGE_NOACCESS,
    PAGE_READONLY,
    PAGE_EXECUTE,
    PAGE_EXECUTE_READ,
    PAGE_READWRITE,
    PAGE_WRITECOPY,
    PAGE_EXECUTE_READWRITE,
    PAGE_EXECUTE_WRITECOPY,
    PAGE_NOACCESS,
    PAGE_NOCACHE | PAGE_READONLY,
    PAGE_NOCACHE | PAGE_EXECUTE,
    PAGE_NOCACHE | PAGE_EXECUTE_READ,
    PAGE_NOCACHE | PAGE_READWRITE,
    PAGE_NOCACHE | PAGE_WRITECOPY,
    PAGE_NOCACHE | PAGE_EXECUTE_READWRITE,
    PAGE_NOCACHE | PAGE_EXECUTE_WRITECOPY,
    PAGE_NOACCESS,
    PAGE_GUARD | PAGE_READONLY,
    PAGE_GUARD | PAGE_EXECUTE,
    PAGE_GUARD | PAGE_EXECUTE_READ,
    PAGE_GUARD | PAGE_READWRITE,
    PAGE_GUARD | PAGE_WRITECOPY,
    PAGE_GUARD | PAGE_EXECUTE_READWRITE,
    PAGE_GUARD | PAGE_EXECUTE_WRITECOPY,
    PAGE_NOACCESS,
    PAGE_WRITECOMBINE | PAGE_READONLY,
    PAGE_WRITECOMBINE | PAGE_EXECUTE,
    PAGE_WRITECOMBINE | PAGE_EXECUTE_READ,
    PAGE_WRITECOMBINE | PAGE_READWRITE,
    PAGE_WRITECOMBINE | PAGE_WRITECOPY,
    PAGE_WRITECOMBINE | PAGE_EXECUTE_READWRITE,
    PAGE_WRITECOMBINE | PAGE_EXECUTE_WRITECOPY
};

/*
* supConvertFromPteProtectionMask
*
* Purpose:
*
* Converts protection from PTE mask.
*
*/
ULONG supConvertFromPteProtectionMask(
    _In_ ULONG ProtectionMask
)
{
    SetLastError(ERROR_SUCCESS);

    if (ProtectionMask < RTL_NUMBER_OF_V2(MmProtectToValue)) {
        return MmProtectToValue[ProtectionMask];
    }
    else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return 0;
}

/*
* supInitMSVCRT
*
* Purpose:
*
* Init MS CRT routines.
*
*/
BOOL supInitMSVCRT(
    VOID
)
{
    HMODULE DllHandle;

    DllHandle = GetModuleHandle(TEXT("ntdll.dll"));

    if (DllHandle) {
        rtl_swprintf_s = (pswprintf_s)GetProcAddress(DllHandle, "swprintf_s");
        rtl_qsort = (pqsort)GetProcAddress(DllHandle, "qsort");
    }

    if (rtl_swprintf_s == NULL ||
        rtl_qsort == NULL)
    {
        DllHandle = GetModuleHandle(TEXT("msvcrt.dll"));
        if (DllHandle == NULL)
            DllHandle = LoadLibraryEx(TEXT("msvcrt.dll"), NULL, 0);

        if (DllHandle) {
            rtl_swprintf_s = (pswprintf_s)GetProcAddress(DllHandle, "swprintf_s");
            rtl_qsort = (pqsort)GetProcAddress(DllHandle, "qsort");
        }
    }

    return ((rtl_swprintf_s != NULL) && (rtl_qsort != NULL));
}

/*
* supHeapAlloc
*
* Purpose:
*
* Wrapper for RtlAllocateHeap with WinObjEx heap.
*
*/
#ifndef _PROFILE_MEMORY_USAGE_
FORCEINLINE PVOID supHeapAlloc(
    _In_ SIZE_T Size)
{
    return RtlAllocateHeap(g_WinObj.Heap, HEAP_ZERO_MEMORY, Size);
}
#else
PVOID supHeapAlloc(
    _In_ SIZE_T Size)
{
    LONG x;
    DWORD LastError;
    PVOID Buffer = NULL;
    WCHAR szBuffer[100];

    Buffer = RtlAllocateHeap(g_WinObj.Heap, HEAP_ZERO_MEMORY, Size);
    LastError = GetLastError();

    if (Buffer) {

        x = InterlockedIncrement((PLONG)&g_cHeapAlloc);

        RtlStringCchPrintfSecure(szBuffer, 100,
            L"supHeapAlloc, block %p with size %llu, g_cHeapAlloc %x\r\n",
            Buffer, Size, x);

        OutputDebugString(szBuffer);
    }
    else {

        RtlStringCchPrintfSecure(szBuffer, 100,
            L"Allocation, block size %llu, FAILED\r\n",
            Size);

        OutputDebugString(szBuffer);
    }

    SetLastError(LastError);
    return Buffer;
}
#endif

/*
* supHeapFree
*
* Purpose:
*
* Wrapper for RtlFreeHeap with WinObjEx heap.
*
*/
#ifndef _PROFILE_MEMORY_USAGE_
FORCEINLINE BOOL supHeapFree(
    _In_ PVOID Memory)
{
    return RtlFreeHeap(g_WinObj.Heap, 0, Memory);
}
#else
BOOL supHeapFree(
    _In_ PVOID Memory)
{
    LONG x;
    BOOL bSuccess;
    DWORD LastError;
    WCHAR szBuffer[100];

    bSuccess = RtlFreeHeap(g_WinObj.Heap, 0, Memory);
    LastError = GetLastError();

    if (bSuccess) {

        x = InterlockedDecrement((PLONG)&g_cHeapAlloc);

        RtlStringCchPrintfSecure(szBuffer, 100,
            L"supHeapFree, block %p, g_cHeapAlloc %x\r\n",
            Memory, x);

        OutputDebugString(szBuffer);
    }
    else {

        RtlStringCchPrintfSecure(szBuffer, 100,
            L"supHeapFree, block %p, FAILED\r\n",
            Memory);

        OutputDebugString(szBuffer);
    }

    SetLastError(LastError);
    return bSuccess;
}
#endif

/*
* supGetDPIValue
*
* Purpose:
*
* Return DPI value for system or specific window (win10+).
*
*/
UINT supGetDPIValue(
    _In_opt_ HWND hWnd
)
{
    HDC hDc;

    UINT uDpi = DefaultSystemDpi;
    DPI_AWARENESS dpiAwareness;

    if (g_NtBuildNumber >= NT_WIN10_REDSTONE1) {

        dpiAwareness = g_ExtApiSet.GetAwarenessFromDpiAwarenessContext(
            g_ExtApiSet.GetThreadDpiAwarenessContext());

        switch (dpiAwareness) {

            // Scale the window to the system DPI
        case DPI_AWARENESS_SYSTEM_AWARE:
            uDpi = g_ExtApiSet.GetDpiForSystem();
            break;

            // Scale the window to the monitor DPI
        case DPI_AWARENESS_PER_MONITOR_AWARE:
            if (hWnd) uDpi = g_ExtApiSet.GetDpiForWindow(hWnd);
            break;
        }

    }
    else {
        hDc = GetDC(0);
        if (hDc) {
            uDpi = (UINT)GetDeviceCaps(hDc, LOGPIXELSX);
            ReleaseDC(0, hDc);
        }
    }

    return uDpi;
}

/*
* supTreeListEnableRedraw
*
* Purpose:
*
* Change treelist redraw state.
*
*/
VOID supTreeListEnableRedraw(
    _In_ HWND TreeList,
    _In_ BOOL fEnable
)
{
    if (fEnable) {
        TreeList_RedrawEnableAndUpdateNow(TreeList);
    }
    else {
        TreeList_RedrawDisable(TreeList);
    }
}

/*
* supTreeListAddItem
*
* Purpose:
*
* Insert new treelist item.
*
*/
HTREEITEM supTreeListAddItem(
    _In_ HWND TreeList,
    _In_opt_ HTREEITEM hParent,
    _In_ UINT mask,
    _In_ UINT state,
    _In_ UINT stateMask,
    _In_opt_ LPWSTR pszText,
    _In_opt_ PVOID subitems
)
{
    TVINSERTSTRUCT  tvitem;
    PTL_SUBITEMS    si = (PTL_SUBITEMS)subitems;

    RtlSecureZeroMemory(&tvitem, sizeof(tvitem));
    tvitem.hParent = hParent;
    tvitem.item.mask = mask;
    tvitem.item.state = state;
    tvitem.item.stateMask = stateMask;
    tvitem.item.pszText = pszText;
    tvitem.hInsertAfter = TVI_LAST;
    return TreeList_InsertTreeItem(TreeList, &tvitem, si);
}

/*
* supInitTreeListForDump
*
* Purpose:
*
* Intialize TreeList control for object dump sheet.
*
*/
BOOL supInitTreeListForDump(
    _In_ HWND hwndParent,
    _Out_ HWND* pTreeListHwnd
)
{
    HWND     TreeList, hWndGroupBox;
    HDITEM   hdritem;
    RECT     rc;

    UINT uDpi;
    INT dpiScaledX, dpiScaledY, iScaledWidth, iScaledHeight, iScaleSub;

    if (pTreeListHwnd == NULL) {
        return FALSE;
    }

    uDpi = supGetDPIValue(NULL);
    dpiScaledX = MulDiv(TreeListDumpObjWndPosX, uDpi, DefaultSystemDpi);
    dpiScaledY = MulDiv(TreeListDumpObjWndPosY, uDpi, DefaultSystemDpi);

    hWndGroupBox = GetDlgItem(hwndParent, ID_OBJECTDUMPGROUPBOX);
    GetWindowRect(hWndGroupBox, &rc);
    iScaleSub = MulDiv(TreeListDumpObjWndScaleSub, uDpi, DefaultSystemDpi);
    iScaledWidth = (rc.right - rc.left) - dpiScaledX - iScaleSub;
    iScaledHeight = (rc.bottom - rc.top) - dpiScaledY - iScaleSub;

    TreeList = CreateWindowEx(WS_EX_STATICEDGE, WC_TREELIST, NULL,
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | TLSTYLE_COLAUTOEXPAND | TLSTYLE_LINKLINES,
        dpiScaledX, dpiScaledY,
        iScaledWidth, iScaledHeight, hwndParent, NULL, NULL, NULL);

    if (TreeList == NULL) {
        *pTreeListHwnd = NULL;
        return FALSE;
    }

    *pTreeListHwnd = TreeList;

    RtlSecureZeroMemory(&hdritem, sizeof(hdritem));
    hdritem.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
    hdritem.fmt = HDF_LEFT | HDF_BITMAP_ON_RIGHT | HDF_STRING;
    hdritem.cxy = SCALE_DPI_VALUE(220, g_WinObj.CurrentDPI);
    hdritem.pszText = TEXT("Field");
    TreeList_InsertHeaderItem(TreeList, 0, &hdritem);
    hdritem.cxy = SCALE_DPI_VALUE(130, g_WinObj.CurrentDPI);
    hdritem.pszText = TEXT("Value");
    TreeList_InsertHeaderItem(TreeList, 1, &hdritem);
    hdritem.cxy = SCALE_DPI_VALUE(210, g_WinObj.CurrentDPI);
    hdritem.pszText = TEXT("Additional Information");
    TreeList_InsertHeaderItem(TreeList, 2, &hdritem);

    return TRUE;
}

/*
* supClipboardCopy
*
* Purpose:
*
* Copy text to the clipboard.
*
*/
VOID supClipboardCopy(
    _In_ LPCWSTR lpText,
    _In_ SIZE_T cbText
)
{
    LPWSTR  lptstrCopy;
    HGLOBAL hglbCopy;
    SIZE_T  dwSize;

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        dwSize = cbText + sizeof(UNICODE_NULL);
        hglbCopy = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwSize);
        if (hglbCopy != NULL) {
            lptstrCopy = (LPWSTR)GlobalLock(hglbCopy);
            if (lptstrCopy) {
                supCopyMemory(lptstrCopy, dwSize, lpText, cbText);
            }
            GlobalUnlock(hglbCopy);
            if (!SetClipboardData(CF_UNICODETEXT, hglbCopy))
                GlobalFree(hglbCopy);
        }
        CloseClipboard();
    }
}

/*
* supQueryObjectFromHandleEx
*
* Purpose:
*
* Return object kernel address from handle in current process handle table.
* Handle table dump supplied as parameter.
*
*/
BOOL supQueryObjectFromHandleEx(
    _In_ PSYSTEM_HANDLE_INFORMATION_EX HandlesDump,
    _In_ HANDLE Object,
    _Out_opt_ ULONG_PTR* Address,
    _Out_opt_ USHORT* TypeIndex
)
{
    USHORT      objectTypeIndex = 0;
    BOOL        bFound = FALSE;
    DWORD       CurrentProcessId = GetCurrentProcessId();
    ULONG_PTR   i, objectAddress = 0;

    for (i = 0; i < HandlesDump->NumberOfHandles; i++) {
        if (HandlesDump->Handles[i].UniqueProcessId == (ULONG_PTR)CurrentProcessId) {
            if (HandlesDump->Handles[i].HandleValue == (ULONG_PTR)Object) {
                if (Address) {
                    objectAddress = (ULONG_PTR)HandlesDump->Handles[i].Object;
                }
                if (TypeIndex) {
                    objectTypeIndex = HandlesDump->Handles[i].ObjectTypeIndex;
                }
                bFound = TRUE;
                break;
            }
        }
    }

    if (Address)
        *Address = objectAddress;
    if (TypeIndex)
        *TypeIndex = objectTypeIndex;

    return bFound;
}

/*
* supQueryObjectFromHandle
*
* Purpose:
*
* Return object kernel address from handle in current process handle table.
*
*/
BOOL supQueryObjectFromHandle(
    _In_ HANDLE Object,
    _Out_ ULONG_PTR* Address,
    _Out_opt_ USHORT* TypeIndex
)
{
    BOOL bFound = FALSE;
    PSYSTEM_HANDLE_INFORMATION_EX pHandles;

    if (Address == NULL) {
        return bFound;
    }

    if (TypeIndex)
        *TypeIndex = 0;

    pHandles = (PSYSTEM_HANDLE_INFORMATION_EX)supGetSystemInfo(SystemExtendedHandleInformation, NULL);
    if (pHandles) {

        bFound = supQueryObjectFromHandleEx(pHandles,
            Object,
            Address,
            TypeIndex);

        supHeapFree(pHandles);
    }

    if (!bFound) *Address = 0;

    return bFound;
}

/*
* supDumpSyscallTableConverted
*
* Purpose:
*
* Read service table and convert it.
*
*/
BOOL supDumpSyscallTableConverted(
    _In_ ULONG_PTR ServiceTableAddress,
    _In_ ULONG ServiceLimit,
    _Out_ PUTable* Table
)
{
    ULONG   ServiceId, memIO;
    BOOL    bResult = FALSE;
    PULONG  ServiceTableDumped = NULL;
    PUTable ConvertedTable;

    LONG32 Offset;

    *Table = NULL;

    memIO = ServiceLimit * sizeof(ULONG);
    ServiceTableDumped = (PULONG)supHeapAlloc(memIO);
    if (ServiceTableDumped) {
        if (kdReadSystemMemoryEx(
            ServiceTableAddress,
            (PVOID)ServiceTableDumped,
            memIO,
            NULL))
        {
            ConvertedTable = (PULONG_PTR)supHeapAlloc(ServiceLimit * sizeof(ULONG_PTR));

            if (ConvertedTable) {

                *Table = ConvertedTable;
                for (ServiceId = 0; ServiceId < ServiceLimit; ServiceId++) {
                    Offset = ((LONG32)ServiceTableDumped[ServiceId] >> 4);
                    ConvertedTable[ServiceId] = ServiceTableAddress + Offset;
                }
                bResult = TRUE;
            }
        }
        supHeapFree(ServiceTableDumped);
    }
    return bResult;
}

/*
* supShowHelp
*
* Purpose:
*
* Display help file if available.
*
*/
VOID supShowHelp(
    _In_ HWND ParentWindow
)
{
    DWORD   dwSize, dwType = 0;
    HKEY    hKey;
    LRESULT lRet;
    HMODULE hHtmlOcx;
    LPWSTR  s;
    WCHAR   szOcxPath[MAX_PATH + 1];
    WCHAR   szBuffer[MAX_PATH * 2];
    WCHAR   szHelpFile[MAX_PATH * 2];

    //
    //  Check if CHM file exist and remember filename.
    //
    RtlSecureZeroMemory(szHelpFile, sizeof(szHelpFile));
    if (!GetCurrentDirectory(MAX_PATH, szHelpFile)) {
        return;
    }
    _strcat(szHelpFile, L"\\winobjex64.chm");

    if (!PathFileExists(szHelpFile)) {
        s = (LPWSTR)supHeapAlloc((MAX_PATH + _strlen(szHelpFile)) * sizeof(WCHAR));
        if (s) {
            _strcpy(s, TEXT("Help file could not be found - "));
            _strcat(s, szHelpFile);
            MessageBox(ParentWindow, s, NULL, MB_ICONINFORMATION);
            supHeapFree(s);
        }
        return;
    }

    //
    // Query OCX path from registry.
    //
    RtlSecureZeroMemory(szOcxPath, sizeof(szOcxPath));
    RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
    lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, HHCTRLOCXKEY, 0, KEY_QUERY_VALUE, &hKey);
    if (lRet == ERROR_SUCCESS) {
        dwSize = MAX_PATH * sizeof(WCHAR);
        lRet = RegQueryValueEx(hKey, L"", NULL, &dwType, (LPBYTE)szBuffer, &dwSize);
        RegCloseKey(hKey);

        if (lRet == ERROR_SUCCESS) {
            if (dwType == REG_EXPAND_SZ) {
                if (ExpandEnvironmentStrings(szBuffer, szOcxPath, MAX_PATH) == 0)
                    lRet = ERROR_SECRET_TOO_LONG;
            }
            else {
                _strncpy(szOcxPath, MAX_PATH, szBuffer, MAX_PATH);
            }
        }
    }
    if (lRet != ERROR_SUCCESS) {
        _strcpy(szOcxPath, HHCTRLOCX);
    }

    //
    // Load OCX and call help.
    //
    hHtmlOcx = GetModuleHandle(HHCTRLOCX);
    if (hHtmlOcx == NULL) {
        hHtmlOcx = LoadLibrary(szOcxPath);
        if (hHtmlOcx == NULL) {
            return;
        }
    }
    if (g_WinObj.HtmlHelpW == NULL) {
        g_WinObj.HtmlHelpW = (pfnHtmlHelpW)GetProcAddress(hHtmlOcx, MAKEINTRESOURCEA(0xF));
        if (g_WinObj.HtmlHelpW == NULL) {
            return;
        }
    }
    g_WinObj.HtmlHelpW(GetDesktopWindow(), szHelpFile, 0, 0);
}

/*
* supEnumIconCallback
*
* Purpose:
*
* Resource enumerator callback.
*
*/
BOOL supEnumIconCallback(
    _In_opt_ HMODULE hModule,
    _In_ LPCWSTR lpType,
    _In_ LPWSTR lpName,
    _In_ LONG_PTR lParam
)
{
    PENUMICONINFO pin;

    UNREFERENCED_PARAMETER(lpType);

    pin = (PENUMICONINFO)lParam;
    if (pin == NULL) {
        return FALSE;
    }

    pin->hIcon = (HICON)LoadImage(hModule, lpName, IMAGE_ICON, pin->cx, pin->cy, 0);
    return FALSE;
}

/*
* supGetMainIcon
*
* Purpose:
*
* Extract main icon if it exists in executable image.
*
*/
HICON supGetMainIcon(
    _In_ LPCWSTR lpFileName,
    _In_ INT cx,
    _In_ INT cy
)
{
    HMODULE      hModule;
    ENUMICONINFO pin;

    pin.cx = cx;
    pin.cy = cy;
    pin.hIcon = 0;

    hModule = LoadLibraryEx(lpFileName, 0, LOAD_LIBRARY_AS_DATAFILE);
    if (hModule != NULL) {
        EnumResourceNames(hModule, RT_GROUP_ICON, (ENUMRESNAMEPROC)&supEnumIconCallback,
            (LONG_PTR)&pin);
        FreeLibrary(hModule);
    }
    return pin.hIcon;
}

/*
* supCopyMemory
*
* Purpose:
*
* Copies bytes between buffers.
*
* dest - Destination buffer
* cbdest - Destination buffer size in bytes
* src - Source buffer
* cbsrc - Source buffer size in bytes
*
*/
void supCopyMemory(
    _Inout_ void* dest,
    _In_ size_t cbdest,
    _In_ const void* src,
    _In_ size_t cbsrc
)
{
    char* d = (char*)dest;
    char* s = (char*)src;

    if ((dest == 0) || (src == 0) || (cbdest == 0))
        return;
    if (cbdest < cbsrc)
        cbsrc = cbdest;

    while (cbsrc > 0) {
        *d++ = *s++;
        cbsrc--;
    }
}

/*
* supSetWaitCursor
*
* Purpose:
*
* Sets cursor state.
*
*/
VOID supSetWaitCursor(
    _In_ BOOL fSet
)
{
    ShowCursor(fSet);
    SetCursor(LoadCursor(NULL, fSet ? IDC_WAIT : IDC_ARROW));
}

typedef struct _SUP_BANNER_DATA {
    LPCWSTR lpText;
    LPCWSTR lpCaption;
    BOOL fList;
} SUP_BANNER_DATA, * PSUP_BANNER_DATA;

/*
* supxLoadBannerDialog
*
* Purpose:
*
* Wait window banner dialog procedure.
*
*/
INT_PTR CALLBACK supxLoadBannerDialog(
    _In_ HWND   hwndDlg,
    _In_ UINT   uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    SUP_BANNER_DATA *pvData;
    UNREFERENCED_PARAMETER(wParam);

    switch (uMsg) {

    case WM_INITDIALOG:

        if (lParam) {
            pvData = (SUP_BANNER_DATA*)lParam;

            if (pvData->fList) {
                SendDlgItemMessage(hwndDlg, IDC_LOADING_MSG, EM_SETLIMITTEXT, 0, 0);
                supCenterWindowPerScreen(hwndDlg);
                if (pvData->lpCaption) SetWindowText(hwndDlg, pvData->lpCaption);
                SendDlgItemMessage(hwndDlg, IDC_LOADING_MSG, EM_REPLACESEL, (WPARAM)0, (LPARAM)pvData->lpText);
            }
            else {
                supCenterWindow(hwndDlg);
                SetDlgItemText(hwndDlg, IDC_LOADING_MSG, (LPWSTR)pvData->lpText);
            }

        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        break;

    default:
        break;
    }
    return 0;
}

/*
* supUpdateLoadBannerText
*
* Purpose:
*
* Set new text for banner window.
*
*/
VOID supUpdateLoadBannerText(
    _In_ HWND hwndBanner,
    _In_ LPCWSTR lpText,
    _In_ BOOL UseList
)
{
    if (UseList) {
        SendDlgItemMessage(hwndBanner, IDC_LOADING_MSG, EM_REPLACESEL, (WPARAM)0, (LPARAM)lpText);
        SendDlgItemMessage(hwndBanner, IDC_LOADING_MSG, EM_REPLACESEL, (WPARAM)0, (LPARAM)(LPWSTR)L"\r\n");
    }
    else {
        SetDlgItemText(hwndBanner, IDC_LOADING_MSG, lpText);
    }
}

/*
* supDisplayLoadBanner
*
* Purpose:
*
* Display borderless banner window to inform user about operation that need some wait.
*
*/
HWND supDisplayLoadBanner(
    _In_opt_ HWND hwndParent,
    _In_ LPCWSTR lpMessage,
    _In_opt_ LPCWSTR lpCaption,
    _In_ BOOL UseList
)
{
    SUP_BANNER_DATA bannerData;

    bannerData.fList = UseList;
    bannerData.lpText = lpMessage;
    bannerData.lpCaption = lpCaption;

    g_hwndBanner = CreateDialogParam(
        g_WinObj.hInstance,
        UseList ? MAKEINTRESOURCE(IDD_DIALOG_LOADLIST) : MAKEINTRESOURCE(IDD_DIALOG_LOAD),
        hwndParent,
        supxLoadBannerDialog,
        (LPARAM)&bannerData);

    if (g_hwndBanner) {
        supSetWaitCursor(TRUE);
        SetCapture(g_hwndBanner);
    }

    return g_hwndBanner;
}

/*
* supCloseLoadBanner
*
* Purpose:
*
* End load banner display.
*
*/
VOID supCloseLoadBanner(
    _In_ HWND hwndBanner
)
{
    if (hwndBanner) {
        SendMessage(hwndBanner, WM_CLOSE, 0, 0);
        g_hwndBanner = NULL;
    }
    supSetWaitCursor(FALSE);
    ReleaseCapture();
}

/*
* supCenterWindow
*
* Purpose:
*
* Centers given window relative to it parent window.
*
*/
VOID supCenterWindow(
    _In_ HWND hwnd
)
{
    RECT rc, rcDlg, rcOwner;
    HWND hwndParent = GetParent(hwnd);

    //center window
    if (hwndParent) {
        GetWindowRect(hwndParent, &rcOwner);
        GetWindowRect(hwnd, &rcDlg);
        CopyRect(&rc, &rcOwner);
        OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
        OffsetRect(&rc, -rc.left, -rc.top);
        OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
        SetWindowPos(hwnd,
            HWND_TOP,
            rcOwner.left + (rc.right / 2),
            rcOwner.top + (rc.bottom / 2),
            0, 0,
            SWP_NOSIZE);
    }
}

/*
* supCenterWindowPerScreen
*
* Purpose:
*
* Centers given window relative to screen.
*
*/
VOID supCenterWindowPerScreen(
    _In_ HWND hwnd
)
{
    RECT rc;
    INT posX = GetSystemMetrics(SM_CXSCREEN);
    INT posY = GetSystemMetrics(SM_CYSCREEN);

    if (GetWindowRect(hwnd, &rc)) {

        posX = (posX - rc.right) / 2;
        posY = (posY - rc.bottom) / 2;

        SetWindowPos(hwnd,
            NULL,
            posX,
            posY,
            0,
            0,
            SWP_NOZORDER | SWP_NOSIZE);
    }
}

/*
* supGetTokenInfo
*
* Purpose:
*
* Returns buffer with token information by given TokenInformationClass.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
PVOID supGetTokenInfo(
    _In_ HANDLE TokenHandle,
    _In_ TOKEN_INFORMATION_CLASS TokenInformationClass,
    _Out_opt_ PULONG ReturnLength
)
{
    PVOID Buffer = NULL;
    ULONG returnLength = 0;

    if (ReturnLength)
        *ReturnLength = 0;

    NtQueryInformationToken(TokenHandle,
        TokenInformationClass,
        NULL,
        0,
        &returnLength);

    Buffer = supHeapAlloc((SIZE_T)returnLength);
    if (Buffer) {

        if (NT_SUCCESS(NtQueryInformationToken(TokenHandle,
            TokenInformationClass,
            Buffer,
            returnLength,
            &returnLength)))
        {
            if (ReturnLength)
                *ReturnLength = returnLength;
            return Buffer;
        }
        else {
            supHeapFree(Buffer);
            return NULL;
        }
    }

    return Buffer;
}

/*
* supGetLoadedModulesList
*
* Purpose:
*
* Read list of loaded kernel modules.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
PVOID supGetLoadedModulesList(
    _Out_opt_ PULONG ReturnLength
)
{
    return ntsupGetLoadedModulesListEx(FALSE,
        ReturnLength,
        supHeapAlloc,
        supHeapFree);
}

/*
* supGetLoadedModulesList2
*
* Purpose:
*
* Read list of loaded kernel modules.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
PVOID supGetLoadedModulesList2(
    _Out_opt_ PULONG ReturnLength
)
{
    return ntsupGetLoadedModulesListEx(TRUE,
        ReturnLength,
        supHeapAlloc,
        supHeapFree);
}

/*
* supGetSystemInfo
*
* Purpose:
*
* Returns buffer with system information by given SystemInformationClass.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
PVOID supGetSystemInfo(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _Out_opt_ PULONG ReturnLength
)
{
    return ntsupGetSystemInfoEx(
        SystemInformationClass,
        ReturnLength,
        (PNTSUPMEMALLOC)supHeapAlloc,
        (PNTSUPMEMFREE)supHeapFree);
}

/*
* supGetObjectTypesInfo
*
* Purpose:
*
* Returns buffer with system types information.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
PVOID supGetObjectTypesInfo(
    VOID
)
{
    PVOID       buffer = NULL;
    ULONG       bufferSize = 1024 * 16;
    NTSTATUS    ntStatus;
    ULONG       returnedLength = 0;

    buffer = supHeapAlloc((SIZE_T)bufferSize);
    if (buffer == NULL)
        return NULL;

    while ((ntStatus = NtQueryObject(
        NULL,
        ObjectTypesInformation,
        buffer,
        bufferSize,
        &returnedLength)) == STATUS_INFO_LENGTH_MISMATCH)
    {
        supHeapFree(buffer);
        bufferSize *= 2;

        if (bufferSize > (16 * 1024 * 1024))
            return NULL;

        buffer = supHeapAlloc((SIZE_T)bufferSize);
    }

    if (NT_SUCCESS(ntStatus)) {
        return buffer;
    }

    if (buffer)
        supHeapFree(buffer);

    return NULL;
}

/*
* supGetItemText
*
* Purpose:
*
* Returns buffer with text from the given listview item.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
LPWSTR supGetItemText(
    _In_ HWND ListView,
    _In_ INT nItem,
    _In_ INT nSubItem,
    _Out_opt_ PSIZE_T lpSize //length in bytes
)
{
    INT     len;
    LPARAM  sz = 0;
    LV_ITEM item;

    RtlSecureZeroMemory(&item, sizeof(item));

    item.iItem = nItem;
    item.iSubItem = nSubItem;
    len = 128;
    do {
        len *= 2;
        item.cchTextMax = len;
        if (item.pszText) {
            supHeapFree(item.pszText);
            item.pszText = NULL;
        }
        item.pszText = (LPWSTR)supHeapAlloc(len * sizeof(WCHAR));
        if (item.pszText == NULL) {
            sz = 0;
            break;
        }
        sz = SendMessage(ListView, LVM_GETITEMTEXT, (WPARAM)item.iItem, (LPARAM)&item);
    } while (sz == (LPARAM)len - 1);

    //empty string
    if (sz == 0) {
        if (item.pszText) {
            supHeapFree(item.pszText);
            item.pszText = NULL;
        }
    }

    if (lpSize) {
        *lpSize = sz * sizeof(WCHAR);
    }
    return item.pszText;
}

/*
* supGetItemText2
*
* Purpose:
*
* Returns text from the given listview item.
*
*/
LPWSTR supGetItemText2(
    _In_ HWND ListView,
    _In_ INT nItem,
    _In_ INT nSubItem,
    _In_ WCHAR* pszText,
    _In_ UINT cchText
)
{
    LV_ITEM item;

    RtlSecureZeroMemory(&item, sizeof(item));

    item.iItem = nItem;
    item.iSubItem = nSubItem;
    item.pszText = pszText;
    item.cchTextMax = (SIZE_T)cchText;
    SendMessage(ListView, LVM_GETITEMTEXT, (WPARAM)item.iItem, (LPARAM)&item);

    return item.pszText;
}

/*
* supLoadImageList
*
* Purpose:
*
* Create and load image list from icon resource type.
*
*/
HIMAGELIST supLoadImageList(
    _In_ HINSTANCE hInst,
    _In_ UINT FirstId,
    _In_ UINT LastId
)
{
    UINT       i;
    HIMAGELIST list;
    HICON      hIcon;

    list = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 8);
    if (list) {
        for (i = FirstId; i <= LastId; i++) {
            hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(i), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
            if (hIcon) {
                ImageList_ReplaceIcon(list, -1, hIcon);
                DestroyIcon(hIcon);
            }
        }
    }
    return list;
}

/*
* supGetObjectNameIndexByTypeIndex
*
* Purpose:
*
* Returns object index of known type.
*
* Known type names listed in objects.c, objects.h
*
*/
UINT supGetObjectNameIndexByTypeIndex(
    _In_ PVOID Object,
    _In_ UCHAR TypeIndex
)
{
    UINT   Index;
    ULONG  i;

    POBJECT_TYPE_INFORMATION pObject;

    union {
        union {
            POBJECT_TYPE_INFORMATION Object;
            POBJECT_TYPE_INFORMATION_V2 ObjectV2;
        } u1;
        PBYTE Ref;
    } ObjectTypeEntry;

    if (Object == NULL ||
        g_pObjectTypesInfo == NULL)
    {
        return ObjectTypeUnknown;
    }

    __try {

        Index = ObDecodeTypeIndex(Object, TypeIndex);

        pObject = OBJECT_TYPES_FIRST_ENTRY(g_pObjectTypesInfo);

        for (i = 0; i < g_pObjectTypesInfo->NumberOfTypes; i++) {
            if (g_NtBuildNumber >= NT_WIN8_RTM) {
                ObjectTypeEntry.Ref = (PBYTE)pObject;
                if (ObjectTypeEntry.u1.ObjectV2->TypeIndex == Index) {

                    return ObManagerGetIndexByTypeName(
                        pObject->TypeName.Buffer);

                }
            }
            else {
                if (i + 2 == Index) {

                    return ObManagerGetIndexByTypeName(
                        pObject->TypeName.Buffer);

                }
            }
            pObject = OBJECT_TYPES_NEXT_ENTRY(pObject);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        supReportAbnormalTermination(__FUNCTIONW__);
        return ObjectTypeUnknown;
    }
    return ObjectTypeUnknown;
}

/*
* supRunAsAdmin
*
* Purpose:
*
* Restarts application requesting full admin rights.
*
*/
VOID supRunAsAdmin(
    VOID
)
{
    SHELLEXECUTEINFO shinfo;
    WCHAR szPath[MAX_PATH + 1];

    RtlSecureZeroMemory(&szPath, sizeof(szPath));
    if (GetModuleFileName(NULL, szPath, MAX_PATH)) {
        RtlSecureZeroMemory(&shinfo, sizeof(shinfo));
        shinfo.cbSize = sizeof(shinfo);
        shinfo.lpVerb = TEXT("runas");
        shinfo.lpFile = szPath;
        shinfo.lpDirectory = g_WinObj.szProgramDirectory;
        shinfo.nShow = SW_SHOW;
        if (ShellExecuteEx(&shinfo)) {
            PostQuitMessage(0);
        }
    }
}

/*
* supShowProperties
*
* Purpose:
*
* Show file properties Windows dialog.
*
*/
VOID supShowProperties(
    _In_ HWND hwndDlg,
    _In_ LPWSTR lpFileName
)
{
    SHELLEXECUTEINFO shinfo;

    if (lpFileName == NULL) {
        return;
    }

    RtlSecureZeroMemory(&shinfo, sizeof(shinfo));
    shinfo.cbSize = sizeof(shinfo);
    shinfo.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
    shinfo.hwnd = hwndDlg;
    shinfo.lpVerb = TEXT("properties");
    shinfo.lpFile = lpFileName;
    shinfo.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&shinfo);
}

/*
* supJumpToFile
*
* Purpose:
*
* Open explorer window for given path.
*
*/
VOID supJumpToFile(
    _In_ LPCWSTR lpFilePath
)
{
    LPITEMIDLIST pidList;
    HRESULT hr = E_FAIL;
    SIZE_T sz;
    LPWSTR lpCommand;
    WCHAR szExplorer[MAX_PATH * 2];

    pidList = ILCreateFromPath(lpFilePath);
    if (pidList) {
        hr = SHOpenFolderAndSelectItems(pidList, 0, NULL, 0);
        ILFree(pidList);
    }

    if (FAILED(hr)) {

        sz = MAX_PATH + _strlen(g_WinObj.szWindowsDirectory);

        lpCommand = (LPWSTR)supHeapAlloc(sz * sizeof(WCHAR));
        if (lpCommand) {
            _strcpy(lpCommand, TEXT(" /select, \""));
            _strcat(lpCommand, lpFilePath);
            _strcat(lpCommand, TEXT("\""));

            _strcpy(szExplorer, g_WinObj.szWindowsDirectory);
            _strcat(szExplorer, TEXT("\\explorer.exe"));

            supShellExecInExplorerProcessEx(szExplorer, lpCommand);
            supHeapFree(lpCommand);
        }

    }
}

/*
* supUserIsFullAdmin
*
* Purpose:
*
* Tests if the current user is admin with full access token.
*
*/
BOOLEAN supUserIsFullAdmin(
    VOID
)
{
    BOOLEAN  bResult = FALSE;
    HANDLE   hToken = NULL;
    NTSTATUS status;
    DWORD    i, Attributes;
    ULONG    ReturnLength = 0;

    PTOKEN_GROUPS pTkGroups;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup = NULL;

    hToken = supGetCurrentProcessToken();
    if (hToken == NULL)
        return FALSE;

    do {
        if (!NT_SUCCESS(RtlAllocateAndInitializeSid(
            &NtAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &AdministratorsGroup)))
        {
            break;
        }

        status = NtQueryInformationToken(hToken, TokenGroups, NULL, 0, &ReturnLength);
        if (status != STATUS_BUFFER_TOO_SMALL)
            break;

        pTkGroups = (PTOKEN_GROUPS)supHeapAlloc((SIZE_T)ReturnLength);
        if (pTkGroups == NULL)
            break;

        status = NtQueryInformationToken(hToken, TokenGroups, pTkGroups, ReturnLength, &ReturnLength);
        if (NT_SUCCESS(status)) {
            if (pTkGroups->GroupCount > 0)
                for (i = 0; i < pTkGroups->GroupCount; i++) {
                    Attributes = pTkGroups->Groups[i].Attributes;
                    if (RtlEqualSid(AdministratorsGroup, pTkGroups->Groups[i].Sid))
                        if (
                            (Attributes & SE_GROUP_ENABLED) &&
                            (!(Attributes & SE_GROUP_USE_FOR_DENY_ONLY))
                            )
                        {
                            bResult = TRUE;
                            break;
                        }
                }
        }
        supHeapFree(pTkGroups);

    } while (FALSE);

    if (AdministratorsGroup != NULL) {
        RtlFreeSid(AdministratorsGroup);
    }

    NtClose(hToken);
    return bResult;
}

/*
* supObjectListGetObjectType
*
* Purpose:
*
* Return object type of given listview entry.
*
*/
WOBJ_OBJECT_TYPE supObjectListGetObjectType(
    _In_ HWND hwndList,
    _In_ INT iItem
)
{
    LVITEM lvItem;

    lvItem.mask = LVIF_PARAM;
    lvItem.iItem = iItem;
    lvItem.iSubItem = 0;
    lvItem.lParam = 0;
    ListView_GetItem(hwndList, &lvItem);

    return (WOBJ_OBJECT_TYPE)lvItem.lParam;
}

/*
* supSetGotoLinkTargetToolButtonState
*
* Purpose:
*
* Enable/Disable Go To Link Target tool button.
*
*/
VOID supSetGotoLinkTargetToolButtonState(
    _In_ HWND hwnd,
    _In_opt_ HWND hwndlv,
    _In_ INT iItem,
    _In_ BOOL bForce,
    _In_ BOOL bForceEnable
)
{
    UINT  uEnable = MF_BYCOMMAND | MF_GRAYED;

    if (bForce) {
        if (bForceEnable)
            uEnable = MF_BYCOMMAND;
    }
    else {
        if (hwndlv) {
            if (ObjectTypeSymbolicLink == supObjectListGetObjectType(hwndlv, iItem)) {
                uEnable = MF_BYCOMMAND;
            }
        }
    }
    EnableMenuItem(GetSubMenu(GetMenu(hwnd), IDMM_OBJECT), ID_OBJECT_GOTOLINKTARGET, uEnable);
}

/*
* supTreeListAddCopyValueItem
*
* Purpose:
*
* Add copy to clipboard menu item depending on hit treelist header item.
*
*/
BOOL supTreeListAddCopyValueItem(
    _In_ HMENU hMenu,
    _In_ HWND hwndTreeList,
    _In_ UINT uId,
    _In_ UINT uPos,
    _In_ LPARAM lParam,
    _In_ INT *pSubItemHit
)
{
    HDHITTESTINFO hti;
    HD_ITEM hdItem;
    WCHAR szHeaderText[MAX_PATH + 1];
    WCHAR szItem[MAX_PATH * 2];

    *pSubItemHit = -1;

    hti.iItem = -1;
    hti.pt.x = LOWORD(lParam);
    hti.pt.y = HIWORD(lParam);
    ScreenToClient(hwndTreeList, &hti.pt);

    hti.pt.y = 1;
    if (TreeList_HeaderHittest(hwndTreeList, &hti) < 0)
        return FALSE;

    RtlSecureZeroMemory(&hdItem, sizeof(hdItem));

    szHeaderText[0] = 0;
    hdItem.mask = HDI_TEXT;

    hdItem.cchTextMax = sizeof(szHeaderText) - 1;

    hdItem.pszText = szHeaderText;
    if (TreeList_GetHeaderItem(hwndTreeList, hti.iItem, &hdItem)) {

        *pSubItemHit = hti.iItem;

        _strcpy(szItem, TEXT("Copy \""));
        _strcat(szItem, szHeaderText);
        _strcat(szItem, TEXT("\""));
        if (InsertMenu(hMenu, uPos, MF_BYCOMMAND, uId, szItem)) {
            return TRUE;
        }
    }

    return FALSE;
}

/*
* supListViewAddCopyValueItem
*
* Purpose:
*
* Add copy to clipboard menu item depending on hit column.
*
*/
BOOL supListViewAddCopyValueItem(
    _In_ HMENU hMenu,
    _In_ HWND hwndLv,
    _In_ UINT uId,
    _In_ UINT uPos,
    _In_ POINT* lpPoint,
    _Out_ INT* pItemHit,
    _Out_ INT* pColumnHit
)
{
    LVHITTESTINFO lvht;
    LVCOLUMN lvc;
    WCHAR szItem[MAX_PATH * 2];
    WCHAR szColumn[MAX_PATH + 1];

    *pColumnHit = -1;
    *pItemHit = -1;

    RtlSecureZeroMemory(&lvht, sizeof(lvht));
    lvht.pt.x = lpPoint->x;
    lvht.pt.y = lpPoint->y;
    ScreenToClient(hwndLv, &lvht.pt);
    if (ListView_SubItemHitTest(hwndLv, &lvht) == -1)
        return FALSE;

    RtlSecureZeroMemory(&lvc, sizeof(lvc));
    RtlSecureZeroMemory(&szColumn, sizeof(szColumn));

    lvc.mask = LVCF_TEXT;
    lvc.pszText = szColumn;
    lvc.cchTextMax = MAX_PATH;
    if (ListView_GetColumn(hwndLv, lvht.iSubItem, &lvc)) {
        _strcpy(szItem, TEXT("Copy \""));
        _strcat(szItem, szColumn);
        _strcat(szItem, TEXT("\""));
        if (InsertMenu(hMenu, uPos, MF_BYCOMMAND, uId, szItem)) {
            *pColumnHit = lvht.iSubItem;
            *pItemHit = lvht.iItem;
            return TRUE;
        }
    }

    return FALSE;
}

/*
* supTreeListCopyItemValueToClipboard
*
* Purpose:
*
* Copy selected treelist item text to the clipboard.
*
*/
BOOL supTreeListCopyItemValueToClipboard(
    _In_ HWND hwndTreeList,
    _In_ INT tlSubItemHit
)
{
    INT         nIndex;
    LPWSTR      lpCopyData = NULL;
    SIZE_T      cbCopyData = 0;
    TVITEMEX    itemex;
    WCHAR       szText[MAX_PATH + 1];

    TL_SUBITEMS_FIXED* pSubItems = NULL;

    szText[0] = 0;
    RtlSecureZeroMemory(&itemex, sizeof(itemex));
    itemex.mask = TVIF_TEXT;
    itemex.hItem = TreeList_GetSelection(hwndTreeList);
    itemex.pszText = szText;
    itemex.cchTextMax = MAX_PATH;

    if (TreeList_GetTreeItem(hwndTreeList, &itemex, &pSubItems)) {

        if ((tlSubItemHit > 0) && (pSubItems != NULL)) {

            nIndex = (tlSubItemHit - 1);
            if (nIndex < (INT)pSubItems->Count) {

                lpCopyData = pSubItems->Text[nIndex];
                cbCopyData = _strlen(lpCopyData) * sizeof(WCHAR);

            }

        }
        else {
            if (tlSubItemHit == 0) {
                lpCopyData = szText;
                cbCopyData = sizeof(szText);
            }
        }

        if (lpCopyData && cbCopyData) {
            supClipboardCopy(lpCopyData, cbCopyData);
            return TRUE;
        }
        else {
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                CloseClipboard();
            }
        }
    }

    return FALSE;
}

/*
* supListViewCopyItemValueToClipboard
*
* Purpose:
*
* Copy selected item text to the clipboard.
*
*/
BOOL supListViewCopyItemValueToClipboard(
    _In_ HWND hwndListView,
    _In_ INT iItem,
    _In_ INT iSubItem
)
{
    SIZE_T cbText;
    LPWSTR lpText;

    if ((iSubItem < 0) || (iItem < 0))
        return FALSE;

    lpText = supGetItemText(hwndListView,
        iItem,
        iSubItem,
        NULL);

    if (lpText) {
        cbText = _strlen(lpText) * sizeof(WCHAR);
        supClipboardCopy(lpText, cbText);
        supHeapFree(lpText);
        return TRUE;
    }
    else {
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            CloseClipboard();
        }
    }

    return FALSE;
}

/*
* supSetMenuIcon
*
* Purpose:
*
* Associates icon data with given menu item.
*
*/
VOID supSetMenuIcon(
    _In_ HMENU hMenu,
    _In_ UINT iItem,
    _In_ HICON hIcon
)
{
    MENUITEMINFO mii;
    RtlSecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_BITMAP | MIIM_DATA;
    mii.hbmpItem = HBMMENU_CALLBACK;
    mii.dwItemData = (ULONG_PTR)hIcon;
    SetMenuItemInfo(hMenu, iItem, FALSE, &mii);
}

/*
* supCreateToolbarButtons
*
* Purpose:
*
* Main window toolbar initialization.
*
*/
VOID supCreateToolbarButtons(
    _In_ HWND hWndToolbar,
    _In_ HIMAGELIST hImageList
)
{
    TBBUTTON tbButtons[] = {
        { 0, ID_OBJECT_PROPERTIES, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, -1 },
        { 10, 0, 0, BTNS_SEP, {0}, 0, -1 },
        { 1, ID_VIEW_REFRESH, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, -1 },
        { 7, ID_VIEW_DISPLAYGRID, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, -1 },
        { 10, 0, 0, BTNS_SEP, {0}, 0, -1 },
        { 2, ID_FIND_FINDOBJECT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, -1 }
    };

    SendMessage(hWndToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
    SendMessage(hWndToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
    SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)RTL_NUMBER_OF(tbButtons), (LPARAM)&tbButtons);
    SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
}

/*
* supSetProcessMitigationImagesPolicy
*
* Purpose:
*
* Enable images policy mitigation.
*
* N.B. Must be called after plugin manager initialization.
*
*/
VOID supSetProcessMitigationImagesPolicy()
{
    PROCESS_MITIGATION_POLICY_INFORMATION policyInfo;

    if (g_WinObj.EnableFullMitigations) {

        policyInfo.Policy = (PROCESS_MITIGATION_POLICY)ProcessSignaturePolicy;
        policyInfo.SignaturePolicy.Flags = 0;
        policyInfo.SignaturePolicy.MicrosoftSignedOnly = TRUE;
        policyInfo.SignaturePolicy.MitigationOptIn = TRUE;

        NtSetInformationProcess(NtCurrentProcess(),
            ProcessMitigationPolicy,
            &policyInfo,
            sizeof(PROCESS_MITIGATION_POLICY_INFORMATION));

        policyInfo.Policy = (PROCESS_MITIGATION_POLICY)ProcessImageLoadPolicy;
        policyInfo.ImageLoadPolicy.Flags = 0;
        policyInfo.ImageLoadPolicy.PreferSystem32Images = TRUE;
        policyInfo.ImageLoadPolicy.NoLowMandatoryLabelImages = TRUE;

        NtSetInformationProcess(NtCurrentProcess(),
            ProcessMitigationPolicy,
            &policyInfo,
            sizeof(PROCESS_MITIGATION_POLICY_INFORMATION));

    }
}

/*
* supxSetProcessMitigationPolicies
*
* Purpose:
*
* Enable mitigations.
*
*/
VOID supxSetProcessMitigationPolicies()
{
    PROCESS_MITIGATION_POLICY_INFORMATION policyInfo;

    if (g_WinObj.EnableFullMitigations) {

        policyInfo.Policy = (PROCESS_MITIGATION_POLICY)ProcessExtensionPointDisablePolicy;
        policyInfo.ExtensionPointDisablePolicy.Flags = 0;
        policyInfo.ExtensionPointDisablePolicy.DisableExtensionPoints = TRUE;

        NtSetInformationProcess(NtCurrentProcess(),
            ProcessMitigationPolicy,
            &policyInfo,
            sizeof(PROCESS_MITIGATION_POLICY_INFORMATION));

        policyInfo.Policy = (PROCESS_MITIGATION_POLICY)ProcessASLRPolicy;
        policyInfo.ASLRPolicy.Flags = 0;
        policyInfo.ASLRPolicy.EnableHighEntropy = TRUE;
        policyInfo.ASLRPolicy.EnableBottomUpRandomization = TRUE;
        policyInfo.ASLRPolicy.EnableForceRelocateImages = TRUE;

        NtSetInformationProcess(NtCurrentProcess(),
            ProcessMitigationPolicy,
            &policyInfo,
            sizeof(PROCESS_MITIGATION_POLICY_INFORMATION));
        /*
        //
        // Disabled due to multiple incompatibilities, including their own HtmlHelp functions
        // Fixes WOX2007-005.
        //

         if (g_NtBuildNumber > 9600) {

             policyInfo.Policy = (PROCESS_MITIGATION_POLICY)ProcessDynamicCodePolicy;
             policyInfo.DynamicCodePolicy.Flags = 0;
             policyInfo.DynamicCodePolicy.ProhibitDynamicCode = TRUE;

             NtSetInformationProcess(NtCurrentProcess(),
                 ProcessMitigationPolicy,
                 &policyInfo,
                 sizeof(PROCESS_MITIGATION_POLICY_INFORMATION));

         }
         */
    }
}

/*
* supInit
*
* Purpose:
*
* Initializes support subset related resources including kldbg subset.
*
* Must be called once during program startup
*
*/
VOID supInit(
    _In_ BOOLEAN IsFullAdmin
)
{
    WCHAR szError[200];
    NTSTATUS status;

    supxSetProcessMitigationPolicies();

#pragma warning(push)
#pragma warning(disable: 6031)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#pragma warning(pop)

    kdInit(IsFullAdmin);

    RtlInitializeCriticalSection(&g_sapiDB.Lock);
    RtlInitializeCriticalSection(&g_scmDB.Lock);

    if (IsFullAdmin) supCreateSCMSnapshot(SERVICE_DRIVER, NULL);
    sapiCreateSetupDBSnapshot();

    g_pObjectTypesInfo = (POBJECT_TYPES_INFORMATION)supGetObjectTypesInfo();

    status = ExApiSetInit();
    if (!NT_SUCCESS(status)) {
        _strcpy(szError, TEXT("ExApiSetInit() failed, 0x"));
        ultohex(status, _strend(szError));
        logAdd(WOBJ_LOG_ENTRY_ERROR, szError);
    }

    supQueryAlpcPortObjectTypeIndex(&g_WinObj.AlpcPortTypeInfo);

    //
    // Remember current DPI value.
    // 
    g_WinObj.CurrentDPI = supGetDPIValue(NULL);
}

/*
* supShutdown
*
* Purpose:
*
* Free support subset related resources.
*
* Must be called once at the end of program execution.
*
*/
VOID supShutdown(
    VOID
)
{
    kdShutdown();

    supFreeSCMSnapshot(NULL);
    sapiFreeSnapshot();

    RtlDeleteCriticalSection(&g_sapiDB.Lock);
    RtlDeleteCriticalSection(&g_scmDB.Lock);

    if (g_pObjectTypesInfo) supHeapFree(g_pObjectTypesInfo);

    SdtFreeGlobals();
}

/*
* supxEnumServicesStatus
*
* Purpose:
*
* Enumerate services status to the buffer.
*
*/
BOOL supxEnumServicesStatus(
    _In_ SC_HANDLE schSCManager,
    _In_ ULONG ServiceType,
    _Out_ PBYTE* Services,
    _Out_ DWORD* ServicesReturned
)
{
    LPBYTE servicesBuffer = NULL;
    DWORD dwSize = 64 * 1024, dwSizeNeeded = 0, dwNumServices = 0, dwResume = 0;
    DWORD dwError = ERROR_SUCCESS;
    DWORD cMaxLoops = 4, cLoop = 0;

    do {

        servicesBuffer = (LPBYTE)supHeapAlloc(dwSize);
        if (servicesBuffer == NULL)
            break;

        dwError = ERROR_SUCCESS;
        dwResume = 0;

        if (!EnumServicesStatusEx(
            schSCManager,
            SC_ENUM_PROCESS_INFO,
            ServiceType,
            SERVICE_STATE_ALL,
            servicesBuffer,
            dwSize,
            &dwSizeNeeded,
            &dwNumServices,
            &dwResume,
            NULL))
        {
            dwError = GetLastError();
            supHeapFree(servicesBuffer);
            dwSize += dwSizeNeeded;
        }

    } while ((dwError == ERROR_MORE_DATA) && (++cLoop < cMaxLoops));

    if ((dwError == ERROR_SUCCESS) && dwNumServices) {
        *ServicesReturned = dwNumServices;
        *Services = servicesBuffer;
    }
    else {
        if (servicesBuffer)
            supHeapFree(servicesBuffer);

        *ServicesReturned = 0;
        *Services = NULL;
    }

    return (dwNumServices > 0);
}

/*
* supCreateSCMSnapshot
*
* Purpose:
*
* Collects SCM information for drivers description.
*
* Use supFreeSCMSnapshot to free returned buffer.
*
*/
BOOL supCreateSCMSnapshot(
    _In_ ULONG ServiceType,
    _Out_opt_ SCMDB* Snapshot
)
{
    SC_HANDLE schSCManager;
    DWORD     dwNumServices = 0;
    PVOID     Services = NULL;

    if (Snapshot) {
        Snapshot->Entries = NULL;
        Snapshot->NumberOfEntries = 0;
    }

    schSCManager = OpenSCManager(NULL, NULL,
        SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

    if (schSCManager) {

        if (supxEnumServicesStatus(schSCManager,
            ServiceType,
            (PBYTE*)&Services,
            &dwNumServices))
        {
            if (Snapshot) {
                Snapshot->Entries = Services;
                Snapshot->NumberOfEntries = dwNumServices;
            }
            else {
                EnterCriticalSection(&g_scmDB.Lock);
                g_scmDB.Entries = Services;
                g_scmDB.NumberOfEntries = dwNumServices;
                LeaveCriticalSection(&g_scmDB.Lock);
            }
        }

        CloseServiceHandle(schSCManager);
    }

    return (dwNumServices > 0);
}

/*
* supFreeSCMSnapshot
*
* Purpose:
*
* Destroys SCM snapshot buffer.
*
*/
VOID supFreeSCMSnapshot(
    _In_opt_ SCMDB* Snapshot)
{
    if (Snapshot) {
        if ((Snapshot->Entries) && (Snapshot->NumberOfEntries))
            supHeapFree(Snapshot->Entries);
        Snapshot->NumberOfEntries = 0;
        Snapshot->Entries = NULL;
    }
    else {
        EnterCriticalSection(&g_scmDB.Lock);
        supHeapFree(g_scmDB.Entries);
        g_scmDB.Entries = NULL;
        g_scmDB.NumberOfEntries = 0;
        LeaveCriticalSection(&g_scmDB.Lock);
    }
}

/*
* sapiQueryDeviceProperty
*
* Purpose:
*
* Query Device Propery from snapshot data.
*
*/
BOOL sapiQueryDeviceProperty(
    _In_ HANDLE SnapshotHeap,
    _In_ HDEVINFO hDevInfo,
    _In_ SP_DEVINFO_DATA* pDevInfoData,
    _In_ ULONG Property,
    _Out_ LPWSTR* PropertyBuffer,
    _Out_opt_ ULONG* PropertyBufferSize
)
{
    BOOL   result;
    DWORD  dataType = 0, dataSize, returnLength = 0;
    LPWSTR lpProperty;

    *PropertyBuffer = NULL;

    if (PropertyBufferSize)
        *PropertyBufferSize = 0;

    dataSize = (1 + MAX_PATH) * sizeof(WCHAR);
    lpProperty = (LPWSTR)RtlAllocateHeap(SnapshotHeap, HEAP_ZERO_MEMORY, dataSize);
    if (lpProperty == NULL)
        return FALSE;

    result = SetupDiGetDeviceRegistryProperty(hDevInfo,
        pDevInfoData,
        Property,
        &dataType,
        (PBYTE)lpProperty,
        dataSize,
        &returnLength);

    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {

        RtlFreeHeap(SnapshotHeap, 0, lpProperty);
        dataSize = returnLength;
        lpProperty = (LPWSTR)RtlAllocateHeap(SnapshotHeap, HEAP_ZERO_MEMORY, dataSize);
        if (lpProperty) {

            result = SetupDiGetDeviceRegistryProperty(hDevInfo,
                pDevInfoData,
                Property,
                &dataType,
                (PBYTE)lpProperty,
                dataSize,
                &returnLength);

        }

    }

    if (!result) {
        if (lpProperty) {
            RtlFreeHeap(SnapshotHeap, 0, lpProperty);
            lpProperty = NULL;
        }
        dataSize = 0;
    }

    *PropertyBuffer = lpProperty;
    if (PropertyBufferSize)
        *PropertyBufferSize = dataSize;
    return result;
}

/*
* sapiCreateSetupDBSnapshot
*
* Purpose:
*
* Collects Setup API information to the linked list.
*
*/
BOOL sapiCreateSetupDBSnapshot(
    VOID
)
{
    BOOL            bResult = FALSE, bFailed = FALSE;
    DWORD           i, ReturnedDataSize = 0;
    SP_DEVINFO_DATA DeviceInfoData;
    PSAPIDBENTRY    Entry;
    HANDLE          Heap;
    HDEVINFO        hDevInfo;

    Heap = RtlCreateHeap(HEAP_GROWABLE, NULL, 0, 0, NULL, NULL);
    if (Heap == NULL) {
        return FALSE;
    }

    if (g_WinObj.IsWine == FALSE) {
        RtlSetHeapInformation(Heap, HeapEnableTerminationOnCorruption, NULL, 0);
    }
    g_sapiDB.HeapHandle = Heap;

    hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo != INVALID_HANDLE_VALUE) {

        InitializeListHead(&g_sapiDB.ListHead);

        RtlSecureZeroMemory(&DeviceInfoData, sizeof(DeviceInfoData));
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++) {

            Entry = (PSAPIDBENTRY)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, sizeof(SAPIDBENTRY));
            if (Entry == NULL) {
                bFailed = TRUE;
                break;
            }

            //
            // Query Device Name.
            //
            sapiQueryDeviceProperty(Heap,
                hDevInfo,
                &DeviceInfoData,
                SPDRP_PHYSICAL_DEVICE_OBJECT_NAME,
                &Entry->lpDeviceName,
                &ReturnedDataSize);

            //
            // Query Device Description.
            //
            sapiQueryDeviceProperty(Heap,
                hDevInfo,
                &DeviceInfoData,
                SPDRP_DEVICEDESC,
                &Entry->lpDeviceDesc,
                &ReturnedDataSize);

            InsertHeadList(&g_sapiDB.ListHead, &Entry->ListEntry);

        }

        SetupDiDestroyDeviceInfoList(hDevInfo);

        if (bFailed == FALSE)
            bResult = TRUE;
    }

    if (bFailed) {
        RtlDestroyHeap(Heap);
        RtlSecureZeroMemory(&g_sapiDB, sizeof(g_sapiDB));
    }
    return bResult;
}

/*
* sapiFreeSnapshot
*
* Purpose:
*
* Destroys snapshot heap and zero linked list.
*
*/
VOID sapiFreeSnapshot(
    VOID
)
{
    EnterCriticalSection(&g_sapiDB.Lock);
    RtlDestroyHeap(g_sapiDB.HeapHandle);
    g_sapiDB.HeapHandle = NULL;
    g_sapiDB.ListHead.Blink = NULL;
    g_sapiDB.ListHead.Flink = NULL;
    LeaveCriticalSection(&g_sapiDB.Lock);
}

/*
* supCallbackShowChildWindow
*
* Purpose:
*
* Makes window controls (in)visible in the given rectangle type dialog
*
*/
BOOL WINAPI supCallbackShowChildWindow(
    _In_ HWND hwnd,
    _In_ LPARAM lParam
)
{
    RECT r1;
    ENUMCHILDWNDDATA* Data = (PENUMCHILDWNDDATA)lParam;

    if (GetWindowRect(hwnd, &r1)) {
        if (PtInRect(&Data->Rect, *(POINT*)&r1))
            ShowWindow(hwnd, Data->nCmdShow);
    }
    return TRUE;
}

/*
* supQueryWinstationDescription
*
* Purpose:
*
* Query predefined window station types, if found equal copy to buffer it friendly name.
*
* Input buffer size must be at least MAX_PATH size.
*
*/
BOOL supQueryWinstationDescription(
    _In_ LPCWSTR lpWindowStationName,
    _Inout_ LPWSTR Buffer,
    _In_ DWORD ccBuffer //size of buffer in chars
)
{
    BOOL    bFound = FALSE;
    LPCWSTR lpType;

    ULONG entryId;

    if (lpWindowStationName == NULL) {
        SetLastError(ERROR_INVALID_NAME);
        return bFound;
    }

    if ((Buffer == NULL) || (ccBuffer < MAX_PATH)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return bFound;
    }

    lpType = NULL;

    for (entryId = 0; entryId < MAX_KNOWN_WINSTA_DESCRIPTIONS; entryId++) {

        if (_strstri(lpWindowStationName,
            g_WinstaDescArray[entryId].lpszWinSta) != NULL)
        {
            lpType = g_WinstaDescArray[entryId].lpszDesc;
            bFound = TRUE;
            break;
        }

    }

    if (lpType == NULL)
        lpType = T_UnknownType;

    _strcpy(Buffer, lpType);
    _strcat(Buffer, TEXT(" logon session"));

    return bFound;
}

#include "props\propDlg.h"
#include "props\propTypeConsts.h"

/*
* supQueryTypeInfo
*
* Purpose:
*
* Query specific type info for output in listview.
*
* Input buffer size must be at least MAX_PATH size.
*
*/
BOOL supQueryTypeInfo(
    _In_ LPCWSTR lpTypeName,
    _Inout_ LPWSTR Buffer,
    _In_ DWORD ccBuffer //size of buffer in chars
)
{
    BOOL  bResult = FALSE;
    ULONG i, nPool;

    POBJECT_TYPE_INFORMATION pObject;

    if (g_pObjectTypesInfo == NULL) {
        SetLastError(ERROR_INTERNAL_ERROR);
        return bResult;
    }
    if ((Buffer == NULL) || (ccBuffer < MAX_PATH)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return bResult;
    }

    __try {

        //
        // Warning: older Wine/Staging incorrectly implement memory structure layout for this structure and therefore will crash.            
        //
        pObject = OBJECT_TYPES_FIRST_ENTRY(g_pObjectTypesInfo);

        for (i = 0; i < g_pObjectTypesInfo->NumberOfTypes; i++) {
            if (_strncmpi(pObject->TypeName.Buffer,
                lpTypeName,
                pObject->TypeName.Length / sizeof(WCHAR)) == 0)
            {
                for (nPool = 0; nPool < MAX_KNOWN_POOL_TYPES; nPool++) {
                    if ((POOL_TYPE)pObject->PoolType == (POOL_TYPE)a_PoolTypes[nPool].dwValue) {

                        _strncpy(
                            Buffer, ccBuffer,
                            a_PoolTypes[nPool].lpDescription,
                            _strlen(a_PoolTypes[nPool].lpDescription)
                        );

                        break;
                    }
                }
                bResult = TRUE;
                break;
            }

            pObject = OBJECT_TYPES_NEXT_ENTRY(pObject);
        }

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        supReportAbnormalTermination(__FUNCTIONW__);
        return FALSE;
    }
    return bResult;
}

/*
* supQueryDeviceDescription
*
* Purpose:
*
* Query device description from Setup API DB dump.
*
* Buffer should be at least MAX_PATH length in chars.
*
*/
BOOL supQueryDeviceDescription(
    _In_ LPCWSTR lpDeviceName,
    _Inout_ LPWSTR Buffer,
    _In_ DWORD ccBuffer //size of buffer in chars
)
{
    BOOL         bResult, bIsRoot;
    SIZE_T       Length;
    LPWSTR       lpFullDeviceName = NULL;
    PLIST_ENTRY  Entry;
    PSAPIDBENTRY Item;

    bResult = FALSE;

    if ((ccBuffer < MAX_PATH) || (Buffer == NULL)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return bResult;
    }

    //
    // Build full device path.
    //
    Length = (4 + _strlen(lpDeviceName) + _strlen(g_WinObj.CurrentObjectPath)) * sizeof(WCHAR);
    lpFullDeviceName = (LPWSTR)supHeapAlloc(Length);
    if (lpFullDeviceName != NULL) {

        // create full path device name for comparison
        _strcpy(lpFullDeviceName, g_WinObj.CurrentObjectPath);
        bIsRoot = (_strcmpi(g_WinObj.CurrentObjectPath, L"\\") == 0);
        if (bIsRoot == FALSE) {
            _strcat(lpFullDeviceName, L"\\");
        }
        _strcat(lpFullDeviceName, lpDeviceName);

        EnterCriticalSection(&g_sapiDB.Lock);

        //
        // Enumerate devices.
        //
        Entry = g_sapiDB.ListHead.Flink;
        while (Entry && Entry != &g_sapiDB.ListHead) {

            Item = CONTAINING_RECORD(Entry, SAPIDBENTRY, ListEntry);
            if (Item->lpDeviceName != NULL) {
                if (_strcmpi(lpFullDeviceName, Item->lpDeviceName) == 0) {
                    if (Item->lpDeviceDesc != NULL) {

                        _strncpy(
                            Buffer,
                            ccBuffer,
                            Item->lpDeviceDesc,
                            _strlen(Item->lpDeviceDesc));

                    }
                    bResult = TRUE;
                    break;
                }
            }

            Entry = Entry->Flink;
        }

        LeaveCriticalSection(&g_sapiDB.Lock);

        supHeapFree(lpFullDeviceName);
    }
    return bResult;
}

/*
* supQueryDriverDescription
*
* Purpose:
*
* Query driver description from SCM dump or from file version info
*
* Buffer should be at least MAX_PATH length in chars.
*
*/
BOOL supQueryDriverDescription(
    _In_ LPCWSTR lpDriverName,
    _Inout_ LPWSTR Buffer,
    _In_ DWORD ccBuffer //size of buffer in chars
)
{
    BOOL    bResult;
    LPWSTR  lpServiceName = NULL;
    LPWSTR  lpDisplayName = NULL;
    LPWSTR  lpRegKey = NULL;
    SIZE_T  i, sz;

    PVOID   vinfo = NULL;
    DWORD   dwSize, dwHandle;
    LRESULT lRet;
    HKEY    hKey = NULL;

    WCHAR   szBuffer[MAX_PATH + 1];
    WCHAR   szImagePath[MAX_PATH + 1];

    LPTRANSLATE	                  lpTranslate = NULL;
    LPENUM_SERVICE_STATUS_PROCESS pInfo = NULL;

    bResult = FALSE;

    if ((ccBuffer < MAX_PATH) || (Buffer == NULL)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return bResult;
    }

    //
    // First attempt - look in SCM database.
    //

    RtlEnterCriticalSection(&g_scmDB.Lock);

    if (g_scmDB.Entries != NULL) {
        pInfo = (LPENUM_SERVICE_STATUS_PROCESS)g_scmDB.Entries;
        for (i = 0; i < g_scmDB.NumberOfEntries; i++) {

            lpServiceName = pInfo[i].lpServiceName;
            if (lpServiceName == NULL)
                continue;

            // not our driver - skip
            if (_strcmpi(lpServiceName, lpDriverName) != 0)
                continue;

            lpDisplayName = pInfo[i].lpDisplayName;
            if (lpDisplayName == NULL)
                continue;

            // driver has the same name as service - skip, there is no description available
            if (_strcmpi(lpDisplayName, lpDriverName) == 0)
                continue;

            sz = _strlen(lpDisplayName);
            _strncpy(Buffer, ccBuffer, lpDisplayName, sz);
            bResult = TRUE;
            break;
        }
    }

    RtlLeaveCriticalSection(&g_scmDB.Lock);

    // second attempt - query through registry and fs
    if (bResult == FALSE) {

        do {
            sz = _strlen(lpDriverName);
            if (sz == 0)
                break;

            sz += supServicesRegPathSize;
            sz = (1 + sz) * sizeof(WCHAR);

            lpRegKey = (LPWSTR)supHeapAlloc(sz);
            if (lpRegKey == NULL)
                break;

            _strcpy(lpRegKey, supServicesRegPath);
            _strcat(lpRegKey, lpDriverName);

            hKey = NULL;
            lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRegKey, 0, KEY_QUERY_VALUE, &hKey);
            if (ERROR_SUCCESS != lRet)
                break;

            RtlSecureZeroMemory(szImagePath, sizeof(szImagePath));
            dwSize = sizeof(szImagePath) - sizeof(UNICODE_NULL);
            lRet = RegQueryValueEx(hKey, TEXT("ImagePath"), NULL, NULL, (LPBYTE)szImagePath, &dwSize);
            RegCloseKey(hKey);

            if (ERROR_SUCCESS == lRet) {

                dwHandle = 0;
                dwSize = GetFileVersionInfoSizeEx(0, szImagePath, &dwHandle);
                if (dwSize == 0)
                    break;

                // allocate memory for version_info structure
                vinfo = supHeapAlloc(dwSize);
                if (vinfo == NULL)
                    break;

                // query it from file
                if (!GetFileVersionInfoEx(0, szImagePath, 0, dwSize, vinfo))
                    break;

                // query codepage and language id info
                dwSize = 0;
                if (!VerQueryValue(vinfo, T_VERSION_TRANSLATION, (LPVOID*)&lpTranslate, (PUINT)&dwSize))
                    break;

                if (dwSize == 0)
                    break;

                // query filedescription from file with given codepage & language id
                RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));

                RtlStringCchPrintfSecure(szBuffer,
                    MAX_PATH,
                    FORMAT_VERSION_DESCRIPTION,
                    lpTranslate[0].wLanguage,
                    lpTranslate[0].wCodePage);

                // finally query pointer to version_info filedescription block data
                lpDisplayName = NULL;
                dwSize = 0;
                bResult = VerQueryValue(vinfo, szBuffer, (LPVOID*)&lpDisplayName, (PUINT)&dwSize);
                if (bResult) {
                    _strncpy(Buffer, ccBuffer, lpDisplayName, dwSize);
                }

            }

        } while (FALSE);

        if (vinfo) {
            supHeapFree(vinfo);
        }
        if (lpRegKey) {
            supHeapFree(lpRegKey);
        }
    }
    return bResult;
}

/*
* supGetVersionInfoFromSection
*
* Purpose:
*
* Return RT_VERSION data and size in VERSION.DLL friendly view.
*
*/
BOOL supGetVersionInfoFromSection(
    _In_ HANDLE SectionHandle,
    _Out_opt_ PDWORD VersionInfoSize,
    _Out_ LPVOID* VersionData
)
{
    HANDLE sectionHandle = NULL;
    VERHEAD* pVerHead = NULL;
    ULONG_PTR idPath[3];
    PBYTE dataPtr = NULL, dllBase = NULL;
    PVOID versionPtr = NULL;
    SIZE_T dllVirtualSize = 0, verSize = 0;
    ULONG_PTR sizeOfData = 0;
    NTSTATUS ntStatus;
    DWORD dwTemp = 0;

    idPath[0] = (ULONG_PTR)RT_VERSION; //type
    idPath[1] = 1;                     //id
    idPath[2] = 0;                     //lang

    if (VersionInfoSize)
        *VersionInfoSize = 0;

    if (VersionData)
        *VersionData = NULL;
    else
        return FALSE; //this param is required

    __try {

        ntStatus = NtDuplicateObject(NtCurrentProcess(),
            SectionHandle,
            NtCurrentProcess(),
            &sectionHandle,
            SECTION_MAP_READ,
            0,
            0);

        if (!NT_SUCCESS(ntStatus)) {
            supReportAPIError(__FUNCTIONW__, ntStatus);
            __leave;
        }

        ntStatus = NtMapViewOfSection(sectionHandle, NtCurrentProcess(), (PVOID*)&dllBase,
            0, 0, NULL, &dllVirtualSize, ViewUnmap, 0, PAGE_READONLY);
        if (!NT_SUCCESS(ntStatus)) {
            supReportAPIError(__FUNCTIONW__, ntStatus);
            __leave;
        }

        ntStatus = LdrResSearchResource(dllBase, (ULONG_PTR*)&idPath, 3, 0,
            (LPVOID*)&dataPtr, (ULONG_PTR*)&sizeOfData, NULL, NULL);
        if (!NT_SUCCESS(ntStatus)) {
            if ((ntStatus != STATUS_RESOURCE_DATA_NOT_FOUND) &&
                (ntStatus != STATUS_RESOURCE_TYPE_NOT_FOUND) &&
                (ntStatus != STATUS_RESOURCE_NAME_NOT_FOUND))
            {
                supReportAPIError(__FUNCTIONW__, ntStatus);
            }
            __leave;
        }

        pVerHead = (VERHEAD*)dataPtr;
        if (pVerHead->wTotLen > sizeOfData) {
            supReportAPIError(__FUNCTIONW__, STATUS_INVALID_BUFFER_SIZE);
            __leave;
        }

        if (pVerHead->vsf.dwSignature != VS_FFI_SIGNATURE) {
            supReportAPIError(__FUNCTIONW__, STATUS_INVALID_IMAGE_FORMAT);
            __leave;
        }

        dwTemp = (DWORD)pVerHead->wTotLen;
        dwTemp = DWORDUP(dwTemp);

        verSize = ((ULONG_PTR)dwTemp * 2) + sizeof(VER2_SIG);

        if (VersionInfoSize)
            *VersionInfoSize = (DWORD)verSize;

        versionPtr = supHeapAlloc(verSize);
        if (versionPtr == NULL) {
            __leave;
        }

        RtlCopyMemory(versionPtr, pVerHead, dwTemp);

        //
        // Do as GetFileVersionInfo does.
        //
        *((PDWORD)((ULONG_PTR)versionPtr + dwTemp)) = VER2_SIG;

        *VersionData = versionPtr;

    }
    __finally {

        if (AbnormalTermination()) {

            dwTemp = 0;

            if (versionPtr)
                supHeapFree(versionPtr);

            supReportAbnormalTermination(__FUNCTIONW__);
        }

        if (dllBase)
            NtUnmapViewOfSection(NtCurrentProcess(), dllBase);

        if (sectionHandle)
            NtClose(sectionHandle);

    }

    return (dwTemp != 0);
}

/*
* supQuerySectionFileInfo
*
* Purpose:
*
* Query section object type File + Image description from version info block
*
* Buffer should be at least MAX_PATH length in chars.
*
*/
BOOL supQuerySectionFileInfo(
    _In_opt_ HANDLE RootDirectoryHandle,
    _In_ PUNICODE_STRING ObjectName,
    _Inout_ LPWSTR Buffer,
    _In_ DWORD ccBuffer //size of buffer in chars
)
{
    BOOL                        bResult;
    HANDLE                      hSection;
    PVOID                       vinfo;
    LPWSTR                      pcValue;
    LPTRANSLATE                 lpTranslate;
    SIZE_T                      cLength = 0;
    NTSTATUS                    status;
    DWORD                       dwInfoSize;
    OBJECT_ATTRIBUTES           Obja;
    SECTION_BASIC_INFORMATION   sbi;
    SECTION_IMAGE_INFORMATION   sii;
    WCHAR                       szQueryBlock[MAX_PATH + 1];

    bResult = FALSE;

    if ((ccBuffer < MAX_PATH) || (Buffer == NULL)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return bResult;
    }

    vinfo = NULL;
    hSection = NULL;

    do {
        //oleaut32.dll does not have FileDescription

        //  open section with query access
        InitializeObjectAttributes(&Obja, ObjectName, OBJ_CASE_INSENSITIVE, RootDirectoryHandle, NULL);
        status = NtOpenSection(&hSection, SECTION_QUERY, &Obja);
        if (!NT_SUCCESS(status))
            break;

        //  query section flags
        RtlSecureZeroMemory(&sbi, sizeof(sbi));
        status = NtQuerySection(hSection, SectionBasicInformation, (PVOID)&sbi, sizeof(sbi), &cLength);
        if (!NT_SUCCESS(status))
            break;

        //  check if section is SEC_IMAGE | SEC_FILE
        if (!((sbi.AllocationAttributes & SEC_IMAGE) && (sbi.AllocationAttributes & SEC_FILE)))
            break;

        // check image machine type
        RtlSecureZeroMemory(&sii, sizeof(sii));
        status = NtQuerySection(hSection, SectionImageInformation, (PVOID)&sii, sizeof(sii), &cLength);
        if (!NT_SUCCESS(status))
            break;

        if (!supGetVersionInfoFromSection(hSection, NULL, &vinfo))
            break;

        if (vinfo == NULL)
            break;

        // query codepage and language id info
        if (!VerQueryValue(vinfo, T_VERSION_TRANSLATION, (LPVOID*)&lpTranslate, (PUINT)&dwInfoSize))
            break;
        if (dwInfoSize == 0)
            break;

        // query filedescription from file with given codepage & language id
        RtlSecureZeroMemory(szQueryBlock, sizeof(szQueryBlock));

        RtlStringCchPrintfSecure(szQueryBlock,
            MAX_PATH,
            FORMAT_VERSION_DESCRIPTION,
            lpTranslate[0].wLanguage,
            lpTranslate[0].wCodePage);

        // finally query pointer to version_info filedescription block data
        pcValue = NULL;
        dwInfoSize = 0;
        bResult = VerQueryValue(vinfo, szQueryBlock, (LPVOID*)&pcValue, (PUINT)&dwInfoSize);
        if (bResult) {
            _strncpy(Buffer, ccBuffer, pcValue, dwInfoSize);
        }

    } while (FALSE);

    if (hSection) NtClose(hSection);
    if (vinfo) supHeapFree(vinfo);
    return bResult;
}

/*
* supOpenDirectoryForObject
*
* Purpose:
*
* Open directory for given object, handle self case.
*
*/
NTSTATUS supOpenDirectoryForObject(
    _Out_ PHANDLE DirectoryHandle,
    _In_ LPCWSTR lpObjectName,
    _In_ LPCWSTR lpDirectory
)
{
    BOOL   needFree = FALSE;
    NTSTATUS ntStatus;
    SIZE_T i, l, rdirLen, ldirSz;
    LPWSTR singleDirName, lookupDirName;

    *DirectoryHandle = NULL;

    if (lpObjectName == NULL)
        return STATUS_INVALID_PARAMETER_2;
    if (lpDirectory == NULL)
        return STATUS_INVALID_PARAMETER_3;

    lookupDirName = (LPWSTR)lpDirectory;

    //
    // 1) Check if object is directory self
    // Extract directory name and compare (case insensitive) with object name
    // Else go to 3
    //
    l = 0;
    rdirLen = _strlen(lookupDirName);
    for (i = 0; i < rdirLen; i++) {
        if (lookupDirName[i] == TEXT('\\'))
            l = i + 1;
    }

    singleDirName = &lookupDirName[l];
    if (_strcmpi(singleDirName, lpObjectName) == 0) {
        //
        //  2) If we are looking for directory, move search directory up
        //  e.g. lpDirectory = \ObjectTypes, lpObjectName = ObjectTypes then lpDirectory = \ 
        //
        ldirSz = rdirLen * sizeof(WCHAR) + sizeof(UNICODE_NULL);
        lookupDirName = (LPWSTR)supHeapAlloc(ldirSz);
        if (lookupDirName == NULL)
            return STATUS_INSUFFICIENT_RESOURCES;

        needFree = TRUE;

        //special case for root 
        if (l == 1) l++;

        supCopyMemory(lookupDirName, ldirSz, lpDirectory, (l - 1) * sizeof(WCHAR));
    }
    //
    // 3) Open directory
    //
    ntStatus = supOpenDirectory(DirectoryHandle, NULL, lookupDirName, DIRECTORY_QUERY);

    if (needFree) {
        supHeapFree(lookupDirName);
    }

    return ntStatus;
}

/*
* supSaveDialogExecute
*
* Purpose:
*
* Display SaveDialog.
*
*/
BOOL supSaveDialogExecute(
    _In_ HWND OwnerWindow,
    _Inout_ LPWSTR SaveFileName,
    _In_ LPCWSTR DialogFilter
)
{
    OPENFILENAME tag1;

    RtlSecureZeroMemory(&tag1, sizeof(OPENFILENAME));

    tag1.lStructSize = sizeof(OPENFILENAME);
    tag1.hwndOwner = OwnerWindow;
    tag1.lpstrFilter = DialogFilter;
    tag1.lpstrFile = SaveFileName;
    tag1.nMaxFile = MAX_PATH;
    tag1.lpstrInitialDir = NULL;
    tag1.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    return GetSaveFileName(&tag1);
}

/*
* supSetListViewSettings
*
* Purpose:
*
* Set listview imagelist, style flags and theme.
*
*/
VOID supSetListViewSettings(
    _In_ HWND hwndLV,
    _In_ DWORD dwExtendedStyle,
    _In_ BOOL fIgnoreGlobalSettings,
    _In_ BOOL fSetTheme,
    _In_opt_ HIMAGELIST hImageList,
    _In_ INT iImageList
)
{
    DWORD dwFlags = dwExtendedStyle;

    if (!fIgnoreGlobalSettings) {
        if (g_WinObj.ListViewDisplayGrid)
            dwFlags |= LVS_EX_GRIDLINES;
    }

    ListView_SetExtendedListViewStyle(hwndLV, dwFlags);

    if (hImageList) {
        ListView_SetImageList(hwndLV, hImageList, iImageList);
    }

    if (fSetTheme) {
        SetWindowTheme(hwndLV, TEXT("Explorer"), NULL);
    }
}

/*
* supGetStockIcon
*
* Purpose:
*
* Retrieve stock icon of given id.
*
*/
HICON supGetStockIcon(
    _In_ SHSTOCKICONID siid,
    _In_ UINT uFlags)
{
    SHSTOCKICONINFO sii;

    RtlSecureZeroMemory(&sii, sizeof(sii));
    sii.cbSize = sizeof(sii);

    if (SHGetStockIconInfo(siid, uFlags, &sii) == S_OK) {
        return sii.hIcon;
    }
    return NULL;
}

//
// Conversion buffer size
//
#define CONVERT_NTNAME_BUFFER_SIZE 512

/*
* supxConvertFileName
*
* Purpose:
*
* Translate Nt path name to Dos path name.
*
*/
BOOL supxConvertFileName(
    _In_ LPWSTR NtFileName,
    _Inout_ LPWSTR DosFileName,
    _In_ SIZE_T ccDosFileName
)
{
    BOOL bFound = FALSE;

    SIZE_T nLen;

    WCHAR szDrive[3];
    WCHAR szName[MAX_PATH];
    WCHAR szTemp[CONVERT_NTNAME_BUFFER_SIZE];
    WCHAR* pszTemp;

    //
    // All input parameters are validated by caller before.
    //

    //
    // Drive template.
    //
    szDrive[0] = L'X';
    szDrive[1] = L':';
    szDrive[2] = 0;

    //
    // Query array of logical disk drive strings.
    //
    szTemp[0] = 0;
    if (GetLogicalDriveStrings(CONVERT_NTNAME_BUFFER_SIZE - 1, szTemp) == 0)
        return FALSE;

    pszTemp = szTemp;

    do {

        //
        // Copy the drive letter to the template string.
        //
        *szDrive = *pszTemp;
        szName[0] = 0;

        //
        // Lookup each device name.
        //
        if (QueryDosDevice(szDrive, szName, MAX_PATH)) {

            nLen = _strlen(szName);

            if (nLen < MAX_PATH) {

                //
                // Match device name.
                //
                bFound = (_strncmpi(NtFileName, szName, nLen) == 0);

                if (bFound) {

                    //
                    // Build output name.
                    //

                    RtlStringCchPrintfSecure(
                        DosFileName,
                        ccDosFileName,
                        TEXT("%ws%ws"),
                        szDrive,
                        NtFileName + nLen);

                }

            }

        }

        //
        // Go to the next NULL character, i.e. the next drive name.
        //
        while (*pszTemp++);

    } while (!bFound && *pszTemp);

    return bFound;
}

/*
* supGetWin32FileName
*
* Purpose:
*
* Query filename by handle.
*
* Input buffer must be at least MAX_PATH length.
*
*/
BOOL supGetWin32FileName(
    _In_ LPCWSTR FileName,
    _Inout_ LPWSTR Win32FileName,
    _In_ SIZE_T ccWin32FileName
)
{
    BOOL                bResult = FALSE;
    NTSTATUS            ntStatus = STATUS_UNSUCCESSFUL;
    HANDLE              hFile = NULL;
    UNICODE_STRING      NtFileName;
    OBJECT_ATTRIBUTES   obja;
    IO_STATUS_BLOCK     iost;

    BYTE* Buffer = NULL;

    if ((Win32FileName == NULL) || (ccWin32FileName < MAX_PATH)) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    RtlInitUnicodeString(&NtFileName, FileName);
    InitializeObjectAttributes(&obja, &NtFileName, OBJ_CASE_INSENSITIVE, 0, NULL);

    ntStatus = NtCreateFile(&hFile,
        SYNCHRONIZE,
        &obja,
        &iost,
        NULL,
        0,
        FILE_SHARE_VALID_FLAGS,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
        NULL, 0);

    if (NT_SUCCESS(ntStatus)) {

        ntStatus = supQueryObjectInformation(hFile,
            ObjectNameInformation,
            (PVOID*)&Buffer,
            NULL,
            (PNTSUPMEMALLOC)supHeapAlloc,
            (PNTSUPMEMFREE)supHeapFree);

        if (NT_SUCCESS(ntStatus)) {

            bResult = supxConvertFileName(((POBJECT_NAME_INFORMATION)Buffer)->Name.Buffer,
                Win32FileName,
                ccWin32FileName);

            supHeapFree(Buffer);
        }

        NtClose(hFile);
    }

    return bResult;
}

/*
* supIsWine
*
* Purpose:
*
* Detect Wine presense.
*
*/
BOOLEAN supIsWine(
    VOID
)
{
    return (is_wine() == 1);
}

/*
* supQuerySecureBootState
*
* Purpose:
*
* Query Firmware type and SecureBoot state if firmware is EFI.
*
*/
BOOLEAN supQuerySecureBootState(
    _Out_ PBOOLEAN pbSecureBoot
)
{
    BOOLEAN bSecureBoot = FALSE;
    HKEY    hKey;
    DWORD   dwState, dwSize, returnLength;
    LSTATUS lRet;

    SYSTEM_SECUREBOOT_INFORMATION sbi;

    if (pbSecureBoot)
        *pbSecureBoot = FALSE;

    //
    // 1) query firmware environment variable, will not work if not fulladmin.
    //
    if (supEnablePrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE, TRUE)) {

        bSecureBoot = FALSE;

        returnLength = GetFirmwareEnvironmentVariable(
            L"SecureBoot",
            L"{8be4df61-93ca-11d2-aa0d-00e098032b8c}",
            &bSecureBoot,
            sizeof(BOOLEAN));

        supEnablePrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE, FALSE);
        if (returnLength != 0) {
            if (pbSecureBoot) {
                *pbSecureBoot = bSecureBoot;
            }
            return TRUE;
        }
    }

    //
    // 2) NtQSI(SystemSecureBootInformation).
    //
    RtlSecureZeroMemory(&sbi, sizeof(sbi));
    if (NT_SUCCESS(NtQuerySystemInformation(SystemSecureBootInformation,
        &sbi,
        sizeof(SYSTEM_SECUREBOOT_INFORMATION),
        &returnLength)))
    {
        if (sbi.SecureBootCapable == FALSE) {
            if (pbSecureBoot)
                *pbSecureBoot = FALSE;
        }
        else {
            if (pbSecureBoot)
                *pbSecureBoot = sbi.SecureBootEnabled;
        }

        return TRUE;
    }

    //
    // 3) Query state from registry.
    //
    hKey = NULL;
    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, T_SECUREBOOTSTATEKEY, 0, KEY_QUERY_VALUE, &hKey);
    if (lRet == ERROR_SUCCESS) {
        dwState = 0;
        dwSize = sizeof(DWORD);
        lRet = RegQueryValueEx(hKey, T_SECUREBOOTSTATEVALUE, NULL, NULL, (LPBYTE)&dwState, &dwSize);
        RegCloseKey(hKey);

        if (lRet == ERROR_SUCCESS) {

            if (pbSecureBoot) {
                *pbSecureBoot = (dwState == 1);
            }
            return TRUE;
        }
    }

    //
    // 4) Query state from user shared data.
    //
    dwState = USER_SHARED_DATA->DbgSecureBootEnabled;
    if (pbSecureBoot) {
        *pbSecureBoot = (dwState == 1);
        return TRUE;
    }  

    return FALSE;
}

/*
* supxGetWindowStationName
*
* Purpose:
*
* Build current windows station object path based on SessionId value from PEB.
*
*/
BOOLEAN supxGetWindowStationName(
    _Out_ UNICODE_STRING* pusWinstaName
)
{
    LPWSTR WindowStationsDir = L"\\Windows\\WindowStations";
    LPWSTR SourceString;
    ULONG SessionId = NtCurrentPeb()->SessionId;

    WCHAR szWinsta[MAX_PATH];

    if (SessionId) {
        _strcpy(szWinsta, L"\\Sessions\\");
        ultostr(SessionId, _strend(szWinsta));
        _strcat(szWinsta, WindowStationsDir);
        SourceString = szWinsta;
    }
    else {
        SourceString = WindowStationsDir;
    }
    return RtlCreateUnicodeString(pusWinstaName, SourceString);
}

/*
* supOpenWindowStationFromContext
*
* Purpose:
*
* Open Window station with hardcoded object path check.
*
*/
HWINSTA supOpenWindowStationFromContext(
    _In_ PROP_OBJECT_INFO* Context,
    _In_ BOOL fInherit,
    _In_ ACCESS_MASK dwDesiredAccess)
{
    HWINSTA hObject = NULL;
    UNICODE_STRING CurrentWinstaDir;
    UNICODE_STRING WinstaDir;

    DWORD LastError = ERROR_ACCESS_DENIED;

    if (supxGetWindowStationName(&CurrentWinstaDir)) {
        RtlInitUnicodeString(&WinstaDir, Context->lpCurrentObjectPath);
        if (RtlEqualUnicodeString(&WinstaDir, &CurrentWinstaDir, TRUE)) {
            hObject = OpenWindowStation(Context->lpObjectName, fInherit, dwDesiredAccess);
            LastError = GetLastError();
        }
        RtlFreeUnicodeString(&CurrentWinstaDir);
    }

    SetLastError(LastError);
    return hObject;
}

/*
* supQueryObjectTrustLabel
*
* Purpose:
*
* Query object trust label protection origin and level.
*
* Note: hObject must be opened with READ_CONTROL.
*
*/
NTSTATUS supQueryObjectTrustLabel(
    _In_ HANDLE hObject,
    _Out_ PULONG ProtectionType,
    _Out_ PULONG ProtectionLevel)
{
    BOOLEAN                         saclPresent = FALSE, saclDefaulted = FALSE;
    ULONG                           i;
    NTSTATUS                        ntStatus;

    PSID                            aceSID;
    PACL                            sacl = NULL;
    PACE_HEADER                     aceHeader;
    PSYSTEM_PROCESS_TRUST_LABEL_ACE ace;

    ACL_SIZE_INFORMATION            aclSize;
    PSECURITY_DESCRIPTOR            pSD = NULL;

    *ProtectionType = 0;
    *ProtectionLevel = 0;

    do {

        //
        // Query Security Descriptor for given object.
        //

        ntStatus = supQuerySecurityInformation(hObject,
            PROCESS_TRUST_LABEL_SECURITY_INFORMATION,
            &pSD,
            NULL);

        if (!NT_SUCCESS(ntStatus)) {
            break;
        }

        //
        // Query SACL from SD.
        //
        ntStatus = RtlGetSaclSecurityDescriptor(pSD,
            &saclPresent,
            &sacl,
            &saclDefaulted);

        if (!NT_SUCCESS(ntStatus)) {
            break;
        }

        if (!sacl) {
            ntStatus = STATUS_INVALID_SID;
            break;
        }

        //
        // Query SACL size.
        //
        ntStatus = RtlQueryInformationAcl(sacl,
            &aclSize,
            sizeof(aclSize),
            AclSizeInformation);

        if (!NT_SUCCESS(ntStatus)) {
            break;
        }

        //
        // Locate trust label ace.
        //
        for (i = 0; i < aclSize.AceCount; i++) {
            if (NT_SUCCESS(RtlGetAce(sacl, i, (PVOID*)&aceHeader))) {
                if (aceHeader->AceType == SYSTEM_PROCESS_TRUST_LABEL_ACE_TYPE) {
                    ace = (SYSTEM_PROCESS_TRUST_LABEL_ACE*)aceHeader;
                    aceSID = (PSID)(&ace->SidStart);
                    *ProtectionType = *RtlSubAuthoritySid(aceSID, 0);
                    *ProtectionLevel = *RtlSubAuthoritySid(aceSID, 1);
                    ntStatus = STATUS_SUCCESS;
                    break;
                }
            }
        }

    } while (FALSE);

    if (pSD) supHeapFree(pSD);

    return ntStatus;
}

/*
* supQueryTokenUserSid
*
* Purpose:
*
* Return SID of given token.
*
* Use supHeapFree to free memory allocated for result.
*
*/
PSID supQueryTokenUserSid(
    _In_ HANDLE ProcessToken
)
{
    PSID resultSid = NULL;
    PTOKEN_USER ptu;
    NTSTATUS status;
    ULONG sidLength = 0, allocLength;

    status = NtQueryInformationToken(
        ProcessToken,
        TokenUser,
        NULL, 0, &sidLength);

    if (status == STATUS_BUFFER_TOO_SMALL) {

        ptu = (PTOKEN_USER)supHeapAlloc(sidLength);

        if (ptu) {

            status = NtQueryInformationToken(
                ProcessToken,
                TokenUser,
                ptu,
                sidLength,
                &sidLength);

            if (NT_SUCCESS(status)) {

                allocLength = SECURITY_MAX_SID_SIZE;
                if (sidLength > allocLength)
                    allocLength = sidLength;

                resultSid = (PSID)supHeapAlloc(allocLength);
                if (resultSid) {

                    status = RtlCopySid(
                        allocLength,
                        resultSid,
                        ptu->User.Sid);

                }
            }

            supHeapFree(ptu);
        }
    }

    return (NT_SUCCESS(status)) ? resultSid : NULL;
}

/*
* supQueryProcessSid
*
* Purpose:
*
* Return SID for the given process.
*
* Use supHeapFree to free memory allocated for result.
*
*/
PSID supQueryProcessSid(
    _In_ HANDLE ProcessHandle
)
{
    HANDLE processToken = NULL;
    PSID resultSid = NULL;

    if (NT_SUCCESS(NtOpenProcessToken(
        ProcessHandle,
        TOKEN_QUERY,
        &processToken)))
    {
        resultSid = supQueryTokenUserSid(processToken);

        NtClose(processToken);
    }

    return resultSid;
}

/*
* supIsImmersiveProcess
*
* Purpose:
*
* Wrapper for IsImmersiveProcess, since it is not present on Win7.
*
*/
BOOL supIsImmersiveProcess(
    _In_ HANDLE hProcess
)
{
    if (g_ExtApiSet.IsImmersiveProcess)
        return g_ExtApiSet.IsImmersiveProcess(hProcess);

    return FALSE;
}

/*
* supIsProtectedProcess
*
* Purpose:
*
* Check if given process is protected process.
*
*/
NTSTATUS supIsProtectedProcess(
    _In_ HANDLE hProcess,
    _Out_ PBOOL pbProtected
)
{
    NTSTATUS ntStatus;
    ULONG requredLength = 0;
    PROCESS_EXTENDED_BASIC_INFORMATION exbi;

    exbi.Size = sizeof(PROCESS_EXTENDED_BASIC_INFORMATION);
    ntStatus = NtQueryInformationProcess(hProcess, ProcessBasicInformation,
        &exbi, sizeof(exbi), &requredLength);
    
    if (NT_SUCCESS(ntStatus)) {
        *pbProtected  = (exbi.IsProtectedProcess != 0);
    }

    return ntStatus;
}

/*
* supIsLocalSystem
*
* Purpose:
*
* pbResult will be set to TRUE if current account is run by system user, FALSE otherwise.
*
* Function return operation status code.
*
*/
NTSTATUS supIsLocalSystem(
    _In_ HANDLE hToken,
    _Out_ PBOOL pbResult)
{
    BOOL                     bResult = FALSE;
    NTSTATUS                 status = STATUS_UNSUCCESSFUL;
    PSID                     SystemSid = NULL, TokenSid = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuth = SECURITY_NT_AUTHORITY;

    //
    // Assume failure.
    //
    if (pbResult)
        *pbResult = FALSE;

    //
    // Get current user SID.
    //
    TokenSid = supQueryTokenUserSid(hToken);
    if (TokenSid == NULL)
        return status;

    //
    // Get System SID.
    //
    status = RtlAllocateAndInitializeSid(
        &NtAuth,
        1,
        SECURITY_LOCAL_SYSTEM_RID,
        0, 0, 0, 0, 0, 0, 0,
        &SystemSid);

    if (NT_SUCCESS(status)) {

        //
        // Compare SIDs.
        //
        bResult = RtlEqualSid(TokenSid, SystemSid);
        RtlFreeSid(SystemSid);
    }

    supHeapFree(TokenSid);

    if (pbResult)
        *pbResult = bResult;

    return status;
}

/*
* supIsLocalServiceSid
*
* Purpose:
*
* Check if given sid is sid of local service.
*
*/
BOOLEAN supIsLocalServiceSid(
    _In_ PSID Sid
)
{
    SID sidLocalService = { SID_REVISION, 1, SECURITY_NT_AUTHORITY, { SECURITY_LOCAL_SERVICE_RID } };

    return RtlEqualSid(&sidLocalService, Sid);
}

/*
* supxGetSystemToken
*
* Purpose:
*
* Find winlogon process and duplicate it token.
*
*/
NTSTATUS supxGetSystemToken(
    _In_ PVOID ProcessList,
    _Out_ PHANDLE SystemToken)
{
    BOOL bSystemToken = FALSE, bEnabled = FALSE;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    ULONG NextEntryDelta = 0;
    HANDLE hObject = NULL;
    HANDLE hToken = NULL;

    ULONG WinlogonSessionId;
    UNICODE_STRING usWinlogon = RTL_CONSTANT_STRING(L"winlogon.exe");

    union {
        PSYSTEM_PROCESSES_INFORMATION Processes;
        PBYTE ListRef;
    } List;

    *SystemToken = NULL;

    WinlogonSessionId = WTSGetActiveConsoleSessionId();
    if (WinlogonSessionId == 0xFFFFFFFF)
        return STATUS_INVALID_SESSION;

    List.ListRef = (PBYTE)ProcessList;

    do {

        List.ListRef += NextEntryDelta;

        if (RtlEqualUnicodeString(&usWinlogon, &List.Processes->ImageName, TRUE)) {

            if (List.Processes->SessionId == WinlogonSessionId) {

                Status = supOpenProcess(
                    List.Processes->UniqueProcessId,
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    &hObject);

                if (NT_SUCCESS(Status)) {

                    Status = NtOpenProcessToken(
                        hObject,
                        TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_IMPERSONATE | TOKEN_QUERY,
                        &hToken);

                    if (NT_SUCCESS(Status)) {

                        Status = supIsLocalSystem(hToken, &bSystemToken);

                        if (NT_SUCCESS(Status) && (bSystemToken)) {

                            Status = supPrivilegeEnabled(hToken, SE_TCB_PRIVILEGE, &bEnabled);
                            if (NT_SUCCESS(Status)) {
                                if (bEnabled) {
                                    NtClose(hObject);
                                    *SystemToken = hToken;
                                    return STATUS_SUCCESS;
                                }
                                else {
                                    Status = STATUS_PRIVILEGE_NOT_HELD;
                                }
                            }
                        }
                        NtClose(hToken);
                    }

                    NtClose(hObject);
                }

            }
        }

        NextEntryDelta = List.Processes->NextEntryDelta;

    } while (NextEntryDelta);

    return Status;
}

/*
* supRunAsLocalSystem
*
* Purpose:
*
* Restart WinObjEx64 in local system account.
*
* Note: Elevated instance required.
*
*/
BOOL supRunAsLocalSystem(
    _In_ HWND hwndParent)
{
    BOOL bSuccess = FALSE;
    NTSTATUS Status;
    PVOID ProcessList;
    ULONG SessionId = NtCurrentPeb()->SessionId, dummy;

    HANDLE hSystemToken = NULL, hPrimaryToken = NULL, hImpersonationToken = NULL;

    BOOLEAN bThreadImpersonated = FALSE;

    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    SECURITY_QUALITY_OF_SERVICE sqos;
    OBJECT_ATTRIBUTES obja;
    TOKEN_PRIVILEGES* TokenPrivileges;

    BYTE TokenPrivBufffer[sizeof(TOKEN_PRIVILEGES) +
        (1 * sizeof(LUID_AND_ATTRIBUTES))];

    WCHAR szApplication[MAX_PATH * 2];

    //
    // Remember our application name.
    //
    RtlSecureZeroMemory(szApplication, sizeof(szApplication));
    GetModuleFileName(NULL, szApplication, MAX_PATH);

    sqos.Length = sizeof(sqos);
    sqos.ImpersonationLevel = SecurityImpersonation;
    sqos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    sqos.EffectiveOnly = FALSE;
    InitializeObjectAttributes(&obja, NULL, 0, NULL, NULL);
    obja.SecurityQualityOfService = &sqos;

    ProcessList = supGetSystemInfo(SystemProcessInformation, NULL);
    if (ProcessList == NULL) {
        MessageBox(
            hwndParent,
            TEXT("Could not allocate process list, abort."),
            PROGRAM_NAME,
            MB_ICONINFORMATION);

        return FALSE;
    }

    //
    // Optionally, enable debug privileges.
    // 
    supEnablePrivilege(SE_DEBUG_PRIVILEGE, TRUE);

    //
    // Get LocalSystem token from winlogon.
    //
    Status = supxGetSystemToken(ProcessList, &hSystemToken);

    supHeapFree(ProcessList);

    do {
        //
        // Check supxGetSystemToken result.
        //
        if (!NT_SUCCESS(Status) || (hSystemToken == NULL)) {

            supShowNtStatus(hwndParent,
                TEXT("No suitable system token found. Make sure you are running as administrator, code 0x"),
                Status);

            break;
        }

        //
        // Duplicate as impersonation token.
        //
        Status = NtDuplicateToken(
            hSystemToken,
            TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY |
            TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_PRIVILEGES,
            &obja,
            FALSE,
            TokenImpersonation,
            &hImpersonationToken);

        if (!NT_SUCCESS(Status)) {

            supShowNtStatus(hwndParent, TEXT("Error duplicating impersonation token, code 0x"), Status);
            break;
        }

        //
        // Duplicate as primary token.
        //
        Status = NtDuplicateToken(
            hSystemToken,
            TOKEN_ALL_ACCESS,
            &obja,
            FALSE,
            TokenPrimary,
            &hPrimaryToken);

        if (!NT_SUCCESS(Status)) {

            supShowNtStatus(hwndParent, TEXT("Error duplicating primary token, code 0x"), Status);
            break;
        }

        //
        // Impersonate system token.
        //
        Status = NtSetInformationThread(
            NtCurrentThread(),
            ThreadImpersonationToken,
            &hImpersonationToken,
            sizeof(HANDLE));

        if (!NT_SUCCESS(Status)) {

            supShowNtStatus(hwndParent, TEXT("Error while impersonating primary token, code 0x"), Status);
            break;
        }

        bThreadImpersonated = TRUE;

        //
        // Turn on AssignPrimaryToken privilege in impersonated token.
        //
        TokenPrivileges = (TOKEN_PRIVILEGES*)&TokenPrivBufffer;
        TokenPrivileges->PrivilegeCount = 1;
        TokenPrivileges->Privileges[0].Luid.LowPart = SE_ASSIGNPRIMARYTOKEN_PRIVILEGE;
        TokenPrivileges->Privileges[0].Luid.HighPart = 0;
        TokenPrivileges->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        Status = NtAdjustPrivilegesToken(
            hImpersonationToken,
            FALSE,
            TokenPrivileges,
            0,
            NULL,
            (PULONG)&dummy);

        if (!NT_SUCCESS(Status)) {
            supShowNtStatus(hwndParent, TEXT("Error adjusting token privileges, code 0x"), Status);
            break;
        }

        //
        // Set session id to primary token.
        //
        Status = NtSetInformationToken(
            hPrimaryToken,
            TokenSessionId,
            &SessionId,
            sizeof(ULONG));

        if (!NT_SUCCESS(Status)) {
            supShowNtStatus(hwndParent, TEXT("Error setting session id, code 0x"), Status);
            break;
        }

        si.cb = sizeof(si);
        GetStartupInfo(&si);

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;

        //
        // Run new instance with prepared primary token.
        //
        bSuccess = CreateProcessAsUser(
            hPrimaryToken,
            szApplication,
            NULL,
            NULL,
            NULL,
            FALSE,
            CREATE_DEFAULT_ERROR_MODE,
            NULL,
            g_WinObj.szProgramDirectory,
            &si,
            &pi);

        if (bSuccess) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else {
            supShowLastError(hwndParent, TEXT("Run as LocalSystem"), GetLastError());
        }

    } while (FALSE);

    if (hImpersonationToken) {
        NtClose(hImpersonationToken);
    }

    //
    // Revert To Self.
    //
    if (bThreadImpersonated) {
        hImpersonationToken = NULL;
        NtSetInformationThread(
            NtCurrentThread(),
            ThreadImpersonationToken,
            (PVOID)&hImpersonationToken,
            sizeof(HANDLE));
    }

    if (hPrimaryToken) NtClose(hPrimaryToken);
    if (hSystemToken) NtClose(hSystemToken);

    //
    // Quit.
    //
    if (bSuccess)
        PostQuitMessage(0);

    return bSuccess;
}

/*
* supAddListViewColumn
*
* Purpose:
*
* Wrapper for ListView_InsertColumn.
*
*/
INT supAddListViewColumn(
    _In_ HWND ListViewHwnd,
    _In_ INT ColumnIndex,
    _In_ INT SubItemIndex,
    _In_ INT OrderIndex,
    _In_ INT ImageIndex,
    _In_ INT Format,
    _In_ LPWSTR Text,
    _In_ INT Width
)
{
    LVCOLUMN column;

    column.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT | LVCF_WIDTH | LVCF_ORDER | LVCF_IMAGE;
    column.fmt = Format;
    column.cx = SCALE_DPI_VALUE(Width, g_WinObj.CurrentDPI);
    column.pszText = Text;
    column.iSubItem = SubItemIndex;
    column.iOrder = OrderIndex;
    column.iImage = ImageIndex;

    return ListView_InsertColumn(ListViewHwnd, ColumnIndex, &column);
}

/*
* supUpdateLvColumnHeaderImage
*
* Purpose:
*
* Set new image for selected column and reset for all the rest.
*
*/
VOID supUpdateLvColumnHeaderImage(
    _In_ HWND ListView,
    _In_ INT NumberOfColumns,
    _In_ INT UpdateColumn,
    _In_ INT ImageIndex
)
{
    INT i;
    LVCOLUMN col;

    RtlSecureZeroMemory(&col, sizeof(col));
    col.mask = LVCF_IMAGE;

    for (i = 0; i < NumberOfColumns; i++) {
        if (i == UpdateColumn) {
            col.iImage = ImageIndex;
        }
        else {
            col.iImage = I_IMAGENONE;
        }
        ListView_SetColumn(ListView, i, &col);
    }
}

/*
* supGetMaxOfTwoU64FromHex
*
* Purpose:
*
* Returned value used in listview comparer functions.
*
*/
INT supGetMaxOfTwoU64FromHex(
    _In_ HWND ListView,
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort,
    _In_ BOOL Inverse
)
{
    INT       nResult;
    LPWSTR    lpItem1 = NULL, lpItem2 = NULL;
    ULONG_PTR ad1, ad2;
    WCHAR     szText[MAX_TEXT_CONVERSION_ULONG64 + 1];

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem1 = supGetItemText2(
        ListView,
        (INT)lParam1,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    ad1 = hextou64(&lpItem1[2]);

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem2 = supGetItemText2(
        ListView,
        (INT)lParam2,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    ad2 = hextou64(&lpItem2[2]);

    if (Inverse)
        nResult = ad1 < ad2;
    else
        nResult = ad1 > ad2;

    return nResult;
}

/*
* supGetMaxOfTwoLongFromString
*
* Purpose:
*
* Returned value used in listview comparer functions.
*
*/
INT supGetMaxOfTwoLongFromString(
    _In_ HWND ListView,
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort,
    _In_ BOOL Inverse
)
{
    INT       nResult;
    LPWSTR    lpItem1 = NULL, lpItem2 = NULL;
    LONG_PTR  value1, value2;
    WCHAR     szText[MAX_TEXT_CONVERSION_ULONG64 + 1];

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem1 = supGetItemText2(
        ListView,
        (INT)lParam1,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    value1 = strtoi64(lpItem1);

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem2 = supGetItemText2(
        ListView,
        (INT)lParam2,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    value2 = strtoi64(lpItem2);

    if (Inverse)
        nResult = value1 < value2;
    else
        nResult = value1 > value2;

    return nResult;
}

/*
* supGetMaxOfTwoULongFromString
*
* Purpose:
*
* Returned value used in listview comparer functions.
*
*/
INT supGetMaxOfTwoULongFromString(
    _In_ HWND ListView,
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort,
    _In_ BOOL Inverse
)
{
    INT       nResult;
    LPWSTR    lpItem1 = NULL, lpItem2 = NULL;
    ULONG_PTR value1, value2;
    WCHAR     szText[MAX_TEXT_CONVERSION_ULONG64 + 1];

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem1 = supGetItemText2(
        ListView,
        (INT)lParam1,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    value1 = strtou64(lpItem1);

    RtlSecureZeroMemory(&szText, sizeof(szText));

    lpItem2 = supGetItemText2(
        ListView,
        (INT)lParam2,
        (INT)lParamSort,
        szText,
        MAX_TEXT_CONVERSION_ULONG64);

    value2 = strtou64(lpItem2);

    if (Inverse)
        nResult = value1 < value2;
    else
        nResult = value1 > value2;

    return nResult;
}

/*
* supGetMaxCompareTwoFixedStrings
*
* Purpose:
*
* Returned value used in listview comparer functions.
*
*/
INT supGetMaxCompareTwoFixedStrings(
    _In_ HWND ListView,
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort,
    _In_ BOOL Inverse
)
{
    INT       nResult;
    LPWSTR    lpItem1 = NULL, lpItem2 = NULL, FirstToCompare, SecondToCompare;
    WCHAR     szString1[MAX_PATH + 1], szString2[MAX_PATH + 1];

    szString1[0] = 0;

    lpItem1 = supGetItemText2(
        ListView,
        (INT)lParam1,
        (INT)lParamSort,
        szString1,
        MAX_PATH);

    szString2[0] = 0;

    lpItem2 = supGetItemText2(
        ListView,
        (INT)lParam2,
        (INT)lParamSort,
        szString2,
        MAX_PATH);

    if (Inverse) {
        FirstToCompare = lpItem2;
        SecondToCompare = lpItem1;
    }
    else {
        FirstToCompare = lpItem1;
        SecondToCompare = lpItem2;
    }

    nResult = _strcmpi(FirstToCompare, SecondToCompare);

    return nResult;
}

/*
* supListViewBaseComparer
*
* Purpose:
*
* Base comparer for listviews.
*
*/
INT supListViewBaseComparer(
    _In_ HWND ListViewHandle,
    _In_ BOOL InverseSort,
    _In_ LPARAM FirstItem,
    _In_ LPARAM SecondItem,
    _In_ LPARAM ColumnToSort
)
{
    INT    nResult = 0;
    LPWSTR lpItem1 = NULL, lpItem2 = NULL, FirstToCompare, SecondToCompare;

    lpItem1 = supGetItemText(ListViewHandle, (INT)FirstItem, (INT)ColumnToSort, NULL);
    lpItem2 = supGetItemText(ListViewHandle, (INT)SecondItem, (INT)ColumnToSort, NULL);

    if ((lpItem1 == NULL) && (lpItem2 == NULL)) {
        nResult = 0;
        goto Done;
    }
    if ((lpItem1 == NULL) && (lpItem2 != NULL)) {
        nResult = (InverseSort) ? 1 : -1;
        goto Done;
    }
    if ((lpItem2 == NULL) && (lpItem1 != NULL)) {
        nResult = (InverseSort) ? -1 : 1;
        goto Done;
    }

    if (InverseSort) {
        FirstToCompare = lpItem2;
        SecondToCompare = lpItem1;
    }
    else {
        FirstToCompare = lpItem1;
        SecondToCompare = lpItem2;
    }

    nResult = _strcmpi(FirstToCompare, SecondToCompare);

Done:
    if (lpItem1) supHeapFree(lpItem1);
    if (lpItem2) supHeapFree(lpItem2);
    return nResult;
}

/*
* supOpenTokenByParam
*
* Purpose:
*
* Open token handle with given desired access for process/thread.
*
*/
NTSTATUS supOpenTokenByParam(
    _In_ CLIENT_ID* ClientId,
    _In_ OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_ ACCESS_MASK TokenDesiredAccess,
    _In_ BOOL IsThreadToken,
    _Out_ PHANDLE TokenHandle)
{
    NTSTATUS Status = STATUS_ACCESS_DENIED;
    HANDLE TokenOwnerHandle = NULL, ObjectHandle = NULL;

    *TokenHandle = NULL;

    if (IsThreadToken) {

        Status = NtOpenThread(&TokenOwnerHandle,
            THREAD_QUERY_INFORMATION,
            ObjectAttributes,
            ClientId);
        if (NT_SUCCESS(Status)) {
            Status = NtOpenThreadToken(TokenOwnerHandle, TokenDesiredAccess, FALSE, &ObjectHandle);
            NtClose(TokenOwnerHandle);
        }

    }
    else {

        Status = supOpenProcess(ClientId->UniqueProcess,
            PROCESS_QUERY_INFORMATION,
            &TokenOwnerHandle);
        if (NT_SUCCESS(Status)) {
            Status = NtOpenProcessToken(TokenOwnerHandle, TokenDesiredAccess, &ObjectHandle);
            NtClose(TokenOwnerHandle);
        }
    }

    *TokenHandle = ObjectHandle;

    return Status;
}

/*
* supOpenDeviceObject
*
* Purpose:
*
* Open handle for device object (NtOpenFile).
*
*/
NTSTATUS supOpenDeviceObject(
    _Out_ PHANDLE ObjectHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
)
{
    IO_STATUS_BLOCK iost;

    return NtOpenFile(ObjectHandle,
        DesiredAccess,
        ObjectAttributes,
        &iost,
        FILE_SHARE_VALID_FLAGS,
        0);
}

/*
* supOpenDeviceObjectEx
*
* Purpose:
*
* Open handle for device object (NtCreateFile).
*
*/
NTSTATUS supOpenDeviceObjectEx(
    _Out_ PHANDLE ObjectHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
)
{
    IO_STATUS_BLOCK iost;

    return NtCreateFile(ObjectHandle,
        DesiredAccess,
        ObjectAttributes,
        &iost,
        NULL,
        0,
        FILE_SHARE_VALID_FLAGS,
        FILE_OPEN,
        0,
        NULL,
        0);
}

/*
* supOpenNamedObjectByType
*
* Purpose:
*
* Open object of supported type and return handle to it.
*
* Supported types are:
*
*  Directory (ObjectName parameter then should be NULL)
*  Device
*  Mutant
*  Key
*  KeyedEvent
*  Semaphore
*  Timer
*  Event
*  EventPair
*  SymbolicLink
*  IoCompletion
*  Section
*  Job
*  Session
*  MemoryPartition
*  AlpcPort
*
*/
NTSTATUS supOpenNamedObjectByType(
    _Out_ HANDLE* ObjectHandle,
    _In_ ULONG TypeIndex,
    _In_ LPCWSTR ObjectDirectory,
    _In_ LPCWSTR ObjectName,
    _In_ ACCESS_MASK DesiredAccess
)
{
    OBJECT_ATTRIBUTES obja;
    UNICODE_STRING ustr;
    HANDLE rootHandle = NULL, objectHandle = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    LPWSTR objectFullName = NULL;
    SIZE_T cchObjectFullName;

    PNTOBJECTOPENPROCEDURE ObjectOpenProcedure = NULL;

    *ObjectHandle = NULL;

    if (ObjectDirectory == NULL)
        return STATUS_INVALID_PARAMETER_3;

    if (ObjectName == NULL)
        return STATUS_INVALID_PARAMETER_4;

    if ((TypeIndex != ObjectTypeDirectory) &&
        (TypeIndex != ObjectTypeDevice) &&
        (TypeIndex != ObjectTypeEvent) &&
        (TypeIndex != ObjectTypeEventPair) &&
        (TypeIndex != ObjectTypeIoCompletion) &&
        (TypeIndex != ObjectTypeJob) &&
        (TypeIndex != ObjectTypeKey) &&
        (TypeIndex != ObjectTypeKeyedEvent) &&
        (TypeIndex != ObjectTypeMutant) &&
        (TypeIndex != ObjectTypeMemoryPartition) &&
        (TypeIndex != ObjectTypePort) &&
        (TypeIndex != ObjectTypeSemaphore) &&
        (TypeIndex != ObjectTypeTimer) &&
        (TypeIndex != ObjectTypeSymbolicLink) &&
        (TypeIndex != ObjectTypeSection) &&
        (TypeIndex != ObjectTypeSession))
    {
        return STATUS_NOT_SUPPORTED;
    }

    __try {

        //
        // Special ALPC port case.
        //
        if (TypeIndex == ObjectTypePort) {

            //
            // Build full object name.
            //
            cchObjectFullName = 4 + _strlen(ObjectDirectory) +
                _strlen(ObjectName) +
                sizeof(UNICODE_NULL);

            objectFullName = (LPWSTR)supHeapAlloc(cchObjectFullName * sizeof(WCHAR));
            if (objectFullName) {

                RtlStringCchPrintfSecure(objectFullName,
                    cchObjectFullName,
                    L"%s\\%s",
                    ObjectDirectory,
                    ObjectName);

                //
                // Open port by name.
                //
                ntStatus = supOpenPortObjectByName(ObjectHandle,
                    DesiredAccess,
                    NULL,
                    objectFullName);

                supHeapFree(objectFullName);

            }

            return ntStatus;
        }

        //
        // Handle directory type.
        //
        if (TypeIndex == ObjectTypeDirectory) {

            //
            // If this is root, then root rootHandle = NULL.
            //       
            if (_strcmpi(ObjectName, KM_OBJECTS_ROOT_DIRECTORY) != 0) {

                ntStatus = supOpenDirectoryForObject(
                    &rootHandle,
                    ObjectName,
                    ObjectDirectory);

                if (!NT_SUCCESS(ntStatus)) {
                    return ntStatus;
                }

            }

            //
            // Open object in directory.
            //
            ntStatus = supOpenDirectory(&objectHandle, rootHandle, ObjectName, DesiredAccess);

            if (rootHandle)
                NtClose(rootHandle);

            *ObjectHandle = objectHandle;
            return ntStatus;
        }

        //
        // Open directory which object belongs.
        //
        RtlInitUnicodeString(&ustr, ObjectDirectory);
        InitializeObjectAttributes(&obja, &ustr, OBJ_CASE_INSENSITIVE, NULL, NULL);

        supOpenDirectoryForObject(&rootHandle, ObjectName, ObjectDirectory);

        RtlInitUnicodeString(&ustr, ObjectName);
        obja.RootDirectory = rootHandle;

        //
        // Select open object procedure.
        //
        switch (TypeIndex) {
        case ObjectTypeDevice:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)supOpenDeviceObject;
            break;

        case ObjectTypeMutant:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenMutant;
            break;

        case ObjectTypeKey:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenKey;
            break;

        case ObjectTypeSemaphore:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenSemaphore;
            break;

        case ObjectTypeTimer:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenTimer;
            break;

        case ObjectTypeEvent:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenEvent;
            break;

        case ObjectTypeEventPair:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenEventPair;
            break;

        case ObjectTypeKeyedEvent:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenKeyedEvent;
            break;

        case ObjectTypeSymbolicLink:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenSymbolicLinkObject;
            break;

        case ObjectTypeIoCompletion:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenIoCompletion;
            break;

        case ObjectTypeSection:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenSection;
            break;

        case ObjectTypeJob:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenJobObject;
            break;

        case ObjectTypeSession:
            ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)NtOpenSession;
            break;

        case ObjectTypeMemoryPartition:
            if (g_ExtApiSet.NtOpenPartition) {
                ObjectOpenProcedure = (PNTOBJECTOPENPROCEDURE)g_ExtApiSet.NtOpenPartition;
            }
            break;
        default:
            ObjectOpenProcedure = NULL;
            break;
        }

        if (ObjectOpenProcedure == NULL) {

            ntStatus = STATUS_PROCEDURE_NOT_FOUND;

        }
        else {

            //
            // Open object of the given type.
            //
            ntStatus = ObjectOpenProcedure(
                &objectHandle,
                DesiredAccess,
                &obja);

            if (NT_SUCCESS(ntStatus))
                *ObjectHandle = objectHandle;
        }

        NtClose(rootHandle);
    }
    __except (WOBJ_EXCEPTION_FILTER_LOG) {
        return STATUS_ACCESS_VIOLATION;
    }

    return ntStatus;
}

/*
* supEnumHandleDump
*
* Purpose:
*
* Execute callback over each handle dump entry.
*
* Return TRUE if enumeration callback stops enumeration.
*
*/
BOOL supEnumHandleDump(
    _In_ PSYSTEM_HANDLE_INFORMATION_EX HandleDump,
    _In_ PENUMERATE_HANDLE_DUMP_CALLBACK EnumCallback,
    _In_ PVOID UserContext
)
{
    ULONG_PTR i;

    for (i = 0; i < HandleDump->NumberOfHandles; i++) {
        if (EnumCallback(&HandleDump->Handles[i],
            UserContext))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*
* supxEnumAlpcPortsCallback
*
* Purpose:
*
* Port handles enumeration callback.
*
*/
BOOL supxEnumAlpcPortsCallback(
    _In_ SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* HandleEntry,
    _In_opt_ PVOID UserContext)
{
    BOOL        fSelfProcess = FALSE, fAlloc = FALSE, bStopEnum = FALSE;
    ULONG       bufferSize = 4096;
    ULONG       returnedLength = 0;
    NTSTATUS    ntStatus;
    HANDLE      objectHandle = NULL, processHandle = NULL;
    BYTE        buffer[4096], * pBuffer = (PBYTE)&buffer;

    PALPCPORT_ENUM_CONTEXT enumContext = (PALPCPORT_ENUM_CONTEXT)UserContext;
    PUNICODE_STRING pusObjectName;

    do {

        if (enumContext == NULL)
            break;

        //
        // Not an ALPC port, skip.
        //
        if (HandleEntry->ObjectTypeIndex != g_WinObj.AlpcPortTypeInfo.AlpcPortTypeIndex)
            break;

        //
        // Not our handle, open process.
        //
        if (HandleEntry->UniqueProcessId != GetCurrentProcessId()) {

            ntStatus = supOpenProcessEx((HANDLE)HandleEntry->UniqueProcessId,
                PROCESS_DUP_HANDLE,
                &processHandle);

            if (!NT_SUCCESS(ntStatus))
                break;

        }
        else {
            //
            // Our handle.
            //
            processHandle = NtCurrentProcess();
            fSelfProcess = TRUE;
        }

        //
        // Duplicate handle.
        //
        ntStatus = NtDuplicateObject(processHandle,
            (HANDLE)HandleEntry->HandleValue,
            NtCurrentProcess(),
            &objectHandle,
            STANDARD_RIGHTS_ALL,
            0,
            0);

        if (!fSelfProcess)
            NtClose(processHandle);

        if (!NT_SUCCESS(ntStatus))
            break;

        //
        // Query object name, static buffer used for performance.
        //
        ntStatus = NtQueryObject(objectHandle,
            ObjectNameInformation,
            pBuffer,
            bufferSize,
            &returnedLength);

        if (ntStatus == STATUS_INFO_LENGTH_MISMATCH) {

            pBuffer = (PBYTE)supHeapAlloc((SIZE_T)returnedLength);
            if (pBuffer) {

                fAlloc = TRUE;

                ntStatus = NtQueryObject(objectHandle,
                    ObjectNameInformation,
                    pBuffer,
                    returnedLength,
                    &returnedLength);

            }

        }

        if (NT_SUCCESS(ntStatus)) {

            pusObjectName = (PUNICODE_STRING)pBuffer;
            if (pusObjectName->Buffer && pusObjectName->Length) {

                if (0 == _strcmpi(enumContext->ObjectFullName,
                    pusObjectName->Buffer))
                {
                    enumContext->ObjectHandle = objectHandle;
                    bStopEnum = TRUE;
                    break;
                }
            }

        }

        NtClose(objectHandle);

    } while (FALSE);

    if (fAlloc && pBuffer)
        supHeapFree(pBuffer);


    //
    // Do not stop enumeration until condition.
    //
    return bStopEnum;
}

/*
* supOpenPortObjectByName
*
* Purpose:
*
* Open handle for ALPC port object type with handle duplication.
*
* NOTE:
* Windows only gives you handle to the port in two cases:
*
* 1. When you create it (NtCreatePort and similar);
* 2. When you connect to the specified port.
*
*/
NTSTATUS supOpenPortObjectByName(
    _Out_ PHANDLE ObjectHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_opt_ PHANDLE ReferenceHandle,
    _In_ LPCWSTR ObjectName
)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    PSYSTEM_HANDLE_INFORMATION_EX pHandles = NULL;
    ALPCPORT_ENUM_CONTEXT enumContext;

    if (ObjectHandle)
        *ObjectHandle = NULL;
    if (ReferenceHandle)
        *ReferenceHandle = NULL;

    do {

        //
        // Allocate handle dump.
        //
        pHandles = (PSYSTEM_HANDLE_INFORMATION_EX)supGetSystemInfo(SystemExtendedHandleInformation,
            NULL);
        if (pHandles == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        //
        // Query AlpcPort type index.
        //
        if (g_WinObj.AlpcPortTypeInfo.Initialized == FALSE) {

            supQueryAlpcPortObjectTypeIndex(&g_WinObj.AlpcPortTypeInfo);

            if (g_WinObj.AlpcPortTypeInfo.Initialized == FALSE) {
                ntStatus = STATUS_PORT_UNREACHABLE;
                break;
            }
        }

        //
           // Walk handle table looking for our named port.
           //
        enumContext.ObjectFullName = ObjectName;
        enumContext.ObjectHandle = NULL;

        if (supEnumHandleDump(pHandles,
            supxEnumAlpcPortsCallback,
            &enumContext))
        {
            if (enumContext.ObjectHandle) {

                //
                // Duplicate copy with requested desired access.
                //
                ntStatus = NtDuplicateObject(NtCurrentProcess(),
                    enumContext.ObjectHandle,
                    NtCurrentProcess(),
                    ObjectHandle,
                    DesiredAccess,
                    0,
                    0);

                if (ReferenceHandle)
                    *ReferenceHandle = enumContext.ObjectHandle;
                else
                    NtClose(enumContext.ObjectHandle);

            }
            else {
                ntStatus = STATUS_INVALID_HANDLE;
            }
        }
        else {
            ntStatus = STATUS_PORT_CONNECTION_REFUSED;
        }

    } while (FALSE);

    if (pHandles)
        supHeapFree(pHandles);

    return ntStatus;
}

/*
* supOpenPortObjectFromContext
*
* Purpose:
*
* Open handle for ALPC port object type with handle duplication using WinObjEx64 property context.
*
*/
NTSTATUS supOpenPortObjectFromContext(
    _Out_ PHANDLE ObjectHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PROP_OBJECT_INFO* Context
)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    HANDLE refHandle = NULL;
    LPWSTR objectFullName = NULL;
    SIZE_T cchObjectFullName;

    *ObjectHandle = NULL;

    /*
    Context->PortObjectInfo.IsAllocated = TRUE;
    Context->PortObjectInfo.ReferenceHandle = TestGetPortHandle();
    */

    if (Context->PortObjectInfo.IsAllocated) {

        ntStatus = NtDuplicateObject(NtCurrentProcess(),
            Context->PortObjectInfo.ReferenceHandle,
            NtCurrentProcess(),
            ObjectHandle,
            DesiredAccess,
            0,
            0);

    }
    else {

        do {

            //
            // Build full object name.
            //
            cchObjectFullName = 4 + _strlen(Context->lpCurrentObjectPath) +
                _strlen(Context->lpObjectName) +
                sizeof(UNICODE_NULL);

            objectFullName = (LPWSTR)supHeapAlloc(cchObjectFullName * sizeof(WCHAR));
            if (objectFullName) {

                RtlStringCchPrintfSecure(objectFullName,
                    cchObjectFullName,
                    L"%s\\%s",
                    Context->lpCurrentObjectPath,
                    Context->lpObjectName);

                //
                // Open port by name.
                //
                ntStatus = supOpenPortObjectByName(ObjectHandle,
                    DesiredAccess,
                    &refHandle,
                    objectFullName);

                if (NT_SUCCESS(ntStatus)) {

                    //
                    // Save handle as reference.
                    //
                    Context->PortObjectInfo.ReferenceHandle = refHandle;
                    Context->PortObjectInfo.IsAllocated = TRUE;
                }

                supHeapFree(objectFullName);
            }
            else {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            }

        } while (FALSE);

    }

    return ntStatus;
}

/*
* supOpenObjectFromContext
*
* Purpose:
*
* Return handle (query rights) for the given named object.
*
*/
HANDLE supOpenObjectFromContext(
    _In_ PROP_OBJECT_INFO* Context,
    _In_ OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ NTSTATUS* Status
)
{
    HANDLE hObject = NULL, hPrivateNamespace = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    OBJECT_ATTRIBUTES objaNamespace;

    if (Context->ContextType == propPrivateNamespace) {

        //
        // Open private namespace.
        //
        InitializeObjectAttributes(&objaNamespace, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);

        ntStatus = NtOpenPrivateNamespace(
            &hPrivateNamespace,
            MAXIMUM_ALLOWED,
            &objaNamespace,
            Context->NamespaceInfo.BoundaryDescriptor);

        if (!NT_SUCCESS(ntStatus)) {
            *Status = ntStatus;
            return NULL;
        }

        //
        // Modify OBJECT_ATTRIBUTES RootDirectory.
        //
        ObjectAttributes->RootDirectory = hPrivateNamespace;
    }

    //
    // Open object of common type.
    //

    switch (Context->TypeIndex) {

    case ObjectTypeProcess:

        if (Context->ContextType == propUnnamed) {

            ntStatus = supOpenProcessEx(
                Context->UnnamedObjectInfo.ClientId.UniqueProcess,
                PROCESS_ALL_ACCESS,
                &hObject);

        }
        else
            ntStatus = STATUS_INVALID_PARAMETER;

        break;

    case ObjectTypeThread:

        if (Context->ContextType == propUnnamed) {

            ntStatus = NtOpenThread(
                &hObject,
                DesiredAccess,
                ObjectAttributes,
                &Context->UnnamedObjectInfo.ClientId);

        }
        else
            ntStatus = STATUS_INVALID_PARAMETER;

        break;

    case ObjectTypeToken:

        if (Context->ContextType == propUnnamed) {

            ntStatus = supOpenTokenByParam(
                &Context->UnnamedObjectInfo.ClientId,
                ObjectAttributes,
                DesiredAccess,
                Context->UnnamedObjectInfo.IsThreadToken,
                &hObject);

        }
        else
            ntStatus = STATUS_INVALID_PARAMETER;

        break;

    case ObjectTypeDevice:

        ntStatus = supOpenDeviceObjectEx(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeMutant:

        ntStatus = NtOpenMutant(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeKey:

        ntStatus = NtOpenKey(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeSemaphore:

        ntStatus = NtOpenSemaphore(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeTimer:

        ntStatus = NtOpenTimer(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeEvent:

        ntStatus = NtOpenEvent(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeEventPair:

        ntStatus = NtOpenEventPair(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeSymbolicLink:

        ntStatus = NtOpenSymbolicLinkObject(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeIoCompletion:

        ntStatus = NtOpenIoCompletion(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeSection:

        ntStatus = NtOpenSection(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeJob:

        ntStatus = NtOpenJobObject(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeSession:

        ntStatus = NtOpenSession(
            &hObject,
            DesiredAccess,
            ObjectAttributes);

        break;

    case ObjectTypeMemoryPartition:

        if (g_ExtApiSet.NtOpenPartition) {

            ntStatus = g_ExtApiSet.NtOpenPartition(
                &hObject,
                DesiredAccess,
                ObjectAttributes);

        }
        else
            ntStatus = STATUS_PROCEDURE_NOT_FOUND;

        break;

    case ObjectTypePort:

        ntStatus = supOpenPortObjectFromContext(
            &hObject,
            DesiredAccess,
            Context);

        break;

    default:
        ntStatus = STATUS_OBJECTID_NOT_FOUND;
        break;
    }

    *Status = ntStatus;

    if (hPrivateNamespace) NtClose(hPrivateNamespace);

    return hObject;
}

/*
* supCloseObjectFromContext
*
* Purpose:
*
* Close handle opened with propOpenCurrentObject.
*
*/
BOOL supCloseObjectFromContext(
    _In_ PROP_OBJECT_INFO* Context,
    _In_ HANDLE hObject
)
{
    BOOL bResult = FALSE;

    if (Context == NULL) {
        if (hObject != NULL)
            bResult = NT_SUCCESS(NtClose(hObject));
        return bResult;
    }

    else {

        switch (Context->TypeIndex) {
        case ObjectTypeWinstation:
            bResult = CloseWindowStation((HWINSTA)hObject);
            break;
        case ObjectTypeDesktop:
            bResult = CloseDesktop((HDESK)hObject);
            break;
        default:
            bResult = NT_SUCCESS(NtClose(hObject));
            break;
        }
    }

    return bResult;
}

/*
* supShowError
*
* Purpose:
*
* Display detailed last error to user.
*
*/
VOID supShowLastError(
    _In_ HWND hWnd,
    _In_ LPCWSTR Source,
    _In_ DWORD LastError
)
{
    LPWSTR lpMsgBuf = NULL;

    if (FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        LastError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMsgBuf,
        0,
        NULL))
    {
        MessageBox(hWnd, lpMsgBuf, Source, MB_TOPMOST | MB_ICONERROR);
        LocalFree(lpMsgBuf);
    }
}

/*
* supShowNtStatus
*
* Purpose:
*
* Display detailed last nt status to user.
*
*/
VOID supShowNtStatus(
    _In_ HWND hWnd,
    _In_ LPCWSTR lpText,
    _In_ NTSTATUS Status
)
{
    PWCHAR lpMsg;
    SIZE_T Length = _strlen(lpText);
    lpMsg = (PWCHAR)supHeapAlloc(Length + 100);
    if (lpMsg) {
        _strcpy(lpMsg, lpText);
        ultohex((ULONG)Status, _strend(lpMsg));
        MessageBox(hWnd, lpMsg, PROGRAM_NAME, MB_ICONERROR);
        supHeapFree(lpMsg);
    }
    else {
        kdDebugPrint("Memory allocation failure\r\n");
    }
}

/*
* supFormatNtError
*
* Purpose:
*
* Format details about NT error to be displayed later.
*
* Uppon success use LocalFree on returned buffer.
*
*/
LPWSTR supFormatNtError(
    _In_ NTSTATUS NtError
)
{
    LPWSTR lpMessage = NULL;

    FormatMessage(
        FORMAT_MESSAGE_FROM_HMODULE |
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        (LPCVOID)GetModuleHandle(L"ntdll.dll"),
        NtError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMessage,
        0,
        NULL);

    return lpMessage;
}

/*
* supBSearch
*
* Purpose:
*
* Binary search, https://github.com/torvalds/linux/blob/master/lib/bsearch.c
*
*/
PVOID supBSearch(
    _In_ PCVOID key,
    _In_ PCVOID base,
    _In_ SIZE_T num,
    _In_ SIZE_T size,
    _In_ int(*cmp)(
        _In_ PCVOID key,
        _In_ PCVOID elt
        )
)
{
    const char* pivot;
    int result;

    while (num > 0) {
        pivot = (char*)base + (num >> 1) * size;
        result = cmp(key, pivot);

        if (result == 0)
            return (void*)pivot;

        if (result > 0) {
            base = pivot + size;
            num--;
        }
        num >>= 1;
    }

    return NULL;
}

/*
* supGetProcessMitigationPolicy
*
* Purpose:
*
* Request process mitigation policy values.
*
*/
_Success_(return != FALSE)
BOOL supGetProcessMitigationPolicy(
    _In_ HANDLE hProcess,
    _In_ PROCESS_MITIGATION_POLICY Policy,
    _In_ SIZE_T Size,
    _Out_writes_bytes_(Size) PVOID Buffer
)
{
    ULONG Length = 0;
    PROCESS_MITIGATION_POLICY_RAW_DATA MitigationPolicy;

    if (Size == sizeof(DWORD)) {

        MitigationPolicy.Policy = (PROCESS_MITIGATION_POLICY)Policy;

        if (NT_SUCCESS(NtQueryInformationProcess(
            hProcess,
            ProcessMitigationPolicy,
            &MitigationPolicy,
            sizeof(PROCESS_MITIGATION_POLICY_RAW_DATA),
            &Length)))
        {
            RtlCopyMemory(Buffer, &MitigationPolicy.Value, Size);
            return TRUE;
        }

    }

    return FALSE;
}

/*
* supGetProcessDepState
*
* Purpose:
*
* Query DEP state for process from ProcessExecuteFlags.
*
*/
_Success_(return != FALSE)
BOOL supGetProcessDepState(
    _In_ HANDLE hProcess,
    _Out_ PPROCESS_MITIGATION_DEP_POLICY DepPolicy
)
{
    ULONG ExecuteFlags = 0;

    if (NT_SUCCESS(NtQueryInformationProcess(
        hProcess,
        ProcessExecuteFlags,
        (PVOID)&ExecuteFlags,
        sizeof(ULONG),
        NULL)))
    {
        if (ExecuteFlags & MEM_EXECUTE_OPTION_ENABLE)
            DepPolicy->Enable = 0;
        else
            DepPolicy->Enable = 1;

        if (ExecuteFlags & MEM_EXECUTE_OPTION_DISABLE_THUNK_EMULATION)
            DepPolicy->DisableAtlThunkEmulation = 1;
        else
            DepPolicy->DisableAtlThunkEmulation = 0;

        if (ExecuteFlags & MEM_EXECUTE_OPTION_PERMANENT)
            DepPolicy->Permanent = 1;
        else
            DepPolicy->Permanent = 0;

        return TRUE;
    }

    return FALSE;
}

/*
* supDeviceIoControlProcExp
*
* Purpose:
*
* Send request to Process Explorer driver.
*
*/
NTSTATUS supDeviceIoControlProcExp(
    _In_ ULONG IoControlCode,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_opt_(OutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength
)
{
    NTSTATUS status;
    HANDLE deviceHandle = NULL;

    UNICODE_STRING usDevName = RTL_CONSTANT_STRING(T_DEVICE_PROCEXP152);
    OBJECT_ATTRIBUTES obja;
    IO_STATUS_BLOCK iost;

    if (g_kdctx.IsFullAdmin == FALSE)
        return STATUS_ACCESS_DENIED;

    InitializeObjectAttributes(&obja, &usDevName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtCreateFile(
        &deviceHandle,
        GENERIC_READ | GENERIC_WRITE,
        &obja,
        &iost,
        NULL,
        0,
        0,
        FILE_OPEN,
        0,
        NULL,
        0);

    if (NT_SUCCESS(status)) {

        status = NtDeviceIoControlFile(
            deviceHandle,
            NULL,
            NULL,
            NULL,
            &iost,
            IoControlCode,
            InputBuffer,
            InputBufferLength,
            OutputBuffer,
            OutputBufferLength);

        NtClose(deviceHandle);
    }
    return status;
}

/*
* supOpenProcessEx
*
* Purpose:
*
* Open process via SysInternals Process Explorer driver.
*
* Desired access: PROCESS_ALL_ACCESS
*
*/
NTSTATUS supOpenProcessEx(
    _In_ HANDLE UniqueProcessId,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE ProcessHandle
)
{
    NTSTATUS status;
    HANDLE processHandle = NULL;

    *ProcessHandle = NULL;

    status = supDeviceIoControlProcExp(
        (ULONG)IOCTL_PE_OPEN_PROCESS,
        (PVOID)&UniqueProcessId,
        sizeof(UniqueProcessId),
        (PVOID)&processHandle,
        sizeof(processHandle));

    if (!NT_SUCCESS(status)) {
        status = supOpenProcess(UniqueProcessId,
            DesiredAccess,
            &processHandle);
    }

    *ProcessHandle = processHandle;
    return status;
}

/*
* supOpenProcessTokenEx
*
* Purpose:
*
* Open process token via SysInternals Process Explorer driver.
*
* Desired access: TOKEN_QUERY
*
*/
NTSTATUS supOpenProcessTokenEx(
    _In_ HANDLE ProcessHandle,
    _Out_ PHANDLE TokenHandle
)
{
    NTSTATUS status;
    HANDLE tokenHandle = NULL;

    *TokenHandle = NULL;

    status = supDeviceIoControlProcExp(
        (ULONG)IOCTL_PE_OPEN_PROCESS_TOKEN,
        (PVOID)&ProcessHandle,
        sizeof(ProcessHandle),
        (PVOID)&tokenHandle,
        sizeof(tokenHandle));

    if (NT_SUCCESS(status))
        *TokenHandle = tokenHandle;

    return status;
}

/*
* supPrintTimeConverted
*
* Purpose:
*
* Print local converted time to string buffer.
*
*/
BOOL supPrintTimeConverted(
    _In_ PLARGE_INTEGER Time,
    _In_ WCHAR * lpszBuffer,
    _In_ SIZE_T cchBuffer
)
{
    FILETIME ConvertedTime;
    TIME_FIELDS TimeFields;

    if ((Time == NULL) || (lpszBuffer == NULL)) return FALSE;
    if (cchBuffer == 0) return FALSE;

    RtlSecureZeroMemory(&ConvertedTime, sizeof(ConvertedTime));
    if (FileTimeToLocalFileTime((PFILETIME)Time, (PFILETIME)&ConvertedTime)) {
        RtlSecureZeroMemory(&TimeFields, sizeof(TimeFields));
        RtlTimeToTimeFields((PLARGE_INTEGER)&ConvertedTime, (PTIME_FIELDS)&TimeFields);

        if (TimeFields.Month - 1 < 0) TimeFields.Month = 1;
        if (TimeFields.Month > 12) TimeFields.Month = 12;

        RtlStringCchPrintfSecure(
            lpszBuffer,
            cchBuffer,
            FORMAT_TIME_DATE_VALUE,
            TimeFields.Hour,
            TimeFields.Minute,
            TimeFields.Second,
            TimeFields.Day,
            g_szMonths[TimeFields.Month - 1],
            TimeFields.Year);

        return TRUE;
    }

    return FALSE;
}

/*
* supGetListViewItemParam
*
* Purpose:
*
* Return ListView item associated parameter.
*
*/
BOOL supGetListViewItemParam(
    _In_ HWND hwndListView,
    _In_ INT itemIndex,
    _Out_ PVOID * outParam
)
{
    LVITEM lvItem;

    *outParam = NULL;

    lvItem.mask = LVIF_PARAM;
    lvItem.iItem = itemIndex;
    lvItem.iSubItem = 0;
    lvItem.lParam = 0;

    if (!ListView_GetItem(hwndListView, &lvItem))
        return FALSE;

    *outParam = (PVOID)lvItem.lParam;

    return TRUE;
}

/*
* supSetMinMaxTrackSize
*
* Purpose:
*
* WM_GETMINMAXINFO handler for dialogs.
*
*/
VOID supSetMinMaxTrackSize(
    _In_ PMINMAXINFO MinMaxInfo,
    _In_ INT MinX,
    _In_ INT MinY,
    _In_ BOOL Scaled
)
{
    if (Scaled) {
        MinMaxInfo->ptMinTrackSize.x = SCALE_DPI_VALUE(MinX, g_WinObj.CurrentDPI);
        MinMaxInfo->ptMinTrackSize.y = SCALE_DPI_VALUE(MinY, g_WinObj.CurrentDPI);
    }
    else {
        MinMaxInfo->ptMinTrackSize.x = MinX;
        MinMaxInfo->ptMinTrackSize.y = MinY;
    }
}

/*
* supGetSidNameUse
*
* Purpose:
*
* Translate SidNameUse to string name.
*
*/
LPWSTR supGetSidNameUse(
    _In_ SID_NAME_USE SidNameUse
)
{
    ULONG nameUse = (ULONG)SidNameUse;

    switch (nameUse) {
    case sidTypeUser:
        return L"User";
    case sidTypeGroup:
        return L"Group";
    case sidTypeDomain:
        return L"Domain";
    case sidTypeAlias:
        return L"Alias";
    case sidTypeWellKnownGroup:
        return L"WellKnownGroup";
    case sidTypeDeletedAccount:
        return L"DeletedAccount";
    case sidTypeInvalid:
        return L"Invalid";
    case sidTypeComputer:
        return L"Computer";
    case sidTypeLogonSession:
        return L"LogonSession";
    case sidTypeLabel:
        return L"Label";
    case sidTypeUnknown:
    default:
        return T_Unknown;
    }
}

/*
* supIntegrityToString
*
* Purpose:
*
* Translate integrity level to string name.
*
*/
LPWSTR supIntegrityToString(
    _In_ DWORD IntegrityLevel
)
{
    LPWSTR lpValue = L"Unknown";

    if (IntegrityLevel == SECURITY_MANDATORY_UNTRUSTED_RID) {
        lpValue = L"Untrusted";
    }
    else if (IntegrityLevel == SECURITY_MANDATORY_LOW_RID) {
        lpValue = L"Low";
    }
    else if (IntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID &&
        IntegrityLevel < SECURITY_MANDATORY_HIGH_RID)
    {
        if (IntegrityLevel == SECURITY_MANDATORY_MEDIUM_PLUS_RID)
            lpValue = L"MediumPlus";
        else
            lpValue = L"Medium";
    }
    else if (IntegrityLevel >= SECURITY_MANDATORY_HIGH_RID &&
        IntegrityLevel < SECURITY_MANDATORY_SYSTEM_RID)
    {
        lpValue = L"High";
    }
    else if (IntegrityLevel >= SECURITY_MANDATORY_SYSTEM_RID &&
        IntegrityLevel < SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    {
        lpValue = L"System";
    }
    else if (IntegrityLevel >= SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    {
        lpValue = L"ProtectedProcess";
    }

    return lpValue;
}

/*
* supLookupSidUserAndDomainEx
*
* Purpose:
*
* Query user and domain name from given sid and policy handle.
*
*/
BOOL supLookupSidUserAndDomainEx(
    _In_ PSID Sid,
    _In_ LSA_HANDLE PolicyHandle,
    _Out_ LPWSTR* lpSidUserAndDomain
)
{
    BOOL bResult = FALSE;
    NTSTATUS Status;
    ULONG Length;
    LPWSTR UserAndDomainName = NULL, P;
    PLSA_REFERENCED_DOMAIN_LIST ReferencedDomains = NULL;
    PLSA_TRANSLATED_NAME Names = NULL;

    *lpSidUserAndDomain = NULL;

    Status = LsaLookupSids(
        PolicyHandle,
        1,
        (PSID*)&Sid,
        (PLSA_REFERENCED_DOMAIN_LIST*)&ReferencedDomains,
        (PLSA_TRANSLATED_NAME*)&Names);

    if ((NT_SUCCESS(Status)) && (Status != STATUS_SOME_NOT_MAPPED)) {

        Length = 0;

        if ((ReferencedDomains != NULL) && (Names != NULL)) {

            Length = 4 + ReferencedDomains->Domains[0].Name.MaximumLength +
                Names->Name.MaximumLength;

            UserAndDomainName = (LPWSTR)supHeapAlloc(Length);
            if (UserAndDomainName) {
                P = UserAndDomainName;
                if (ReferencedDomains->Domains[0].Name.Length) {
                    RtlCopyMemory(UserAndDomainName,
                        ReferencedDomains->Domains[0].Name.Buffer,
                        ReferencedDomains->Domains[0].Name.Length);

                    P = _strcat(UserAndDomainName, TEXT("\\"));
                }

                RtlCopyMemory(P,
                    Names->Name.Buffer,
                    Names->Name.Length);

                *lpSidUserAndDomain = UserAndDomainName;
                bResult = TRUE;
            }
        }
        if (ReferencedDomains) LsaFreeMemory(ReferencedDomains);
        if (Names) LsaFreeMemory(Names);
    }

    return bResult;
}

/*
* supLookupSidUserAndDomain
*
* Purpose:
*
* Query user and domain name from given sid.
*
*/
BOOL supLookupSidUserAndDomain(
    _In_ PSID Sid,
    _Out_ LPWSTR * lpSidUserAndDomain
)
{
    BOOL bResult = FALSE;
    LSA_HANDLE PolicyHandle = NULL;

    *lpSidUserAndDomain = NULL;

    if (NT_SUCCESS(supLsaOpenMachinePolicy(POLICY_LOOKUP_NAMES,
        &PolicyHandle)))
    {
        bResult = supLookupSidUserAndDomainEx(Sid,
            PolicyHandle,
            lpSidUserAndDomain);

        LsaClose(PolicyHandle);
    }

    return bResult;
}

/*
* supLsaOpenMachinePolicy
*
* Purpose:
*
* Open local machine policy.
*
*/
NTSTATUS supLsaOpenMachinePolicy(
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PLSA_HANDLE PolicyHandle
)
{
    LSA_OBJECT_ATTRIBUTES lobja;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;

    SecurityQualityOfService.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    SecurityQualityOfService.ImpersonationLevel = SecurityImpersonation;
    SecurityQualityOfService.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    SecurityQualityOfService.EffectiveOnly = FALSE;

    InitializeObjectAttributes(
        &lobja,
        NULL,
        0L,
        NULL,
        NULL);

    lobja.SecurityQualityOfService = &SecurityQualityOfService;

    return LsaOpenPolicy(
        NULL,
        (PLSA_OBJECT_ATTRIBUTES)&lobja,
        DesiredAccess,
        PolicyHandle);
}

/*
* supxHandlesLookupCallback
*
* Purpose:
*
* qsort, bsearch callback.
*
*/
int __cdecl supxHandlesLookupCallback(
    void const* first,
    void const* second
)
{
    int i;
    PSUP_HANDLE_DUMP_ENTRY elem1 = (PSUP_HANDLE_DUMP_ENTRY)first;
    PSUP_HANDLE_DUMP_ENTRY elem2 = (PSUP_HANDLE_DUMP_ENTRY)second;

    if (elem1->HandleValue == elem2->HandleValue)
        i = 0;
    else
        if (elem1->HandleValue < elem2->HandleValue)
            i = -1;
        else
            i = 1;

    return i;
}

/*
* supxHandlesLookupCallback2
*
* Purpose:
*
* qsort, bsearch callback.
*
*/
int __cdecl supxHandlesLookupCallback2(
    void const* first,
    void const* second
)
{
    int i;
    PSUP_HANDLE_DUMP_ENTRY elem1 = (PSUP_HANDLE_DUMP_ENTRY)first;
    PSUP_HANDLE_DUMP_ENTRY elem2 = (PSUP_HANDLE_DUMP_ENTRY)second;
    
    ULONG_PTR FirstObject = (ULONG_PTR)elem1->Object;
    ULONG_PTR SecondObject = (ULONG_PTR)elem2->Object;

    if (FirstObject == SecondObject)
        i = 0;
    else
        if (FirstObject < SecondObject)
            i = -1;
        else
            i = 1;

    return i;
}

/*
* supHandlesCreateFilteredAndSortedList
*
* Purpose:
*
* Create sorted handles list of given process.
*
* Use supHeapFree to release allocated memory.
*
*/
PSUP_HANDLE_DUMP supHandlesCreateFilteredAndSortedList(
    _In_ ULONG_PTR FilterUniqueProcessId,
    _In_ BOOLEAN fObject
)
{
    PSYSTEM_HANDLE_INFORMATION_EX handleDump;
    PSUP_HANDLE_DUMP resultSnapshot;
    ULONG_PTR i, cLast = 0;

    ULONG returnLength = 0;
    SIZE_T stBufferSize;

    handleDump = (PSYSTEM_HANDLE_INFORMATION_EX)ntsupGetSystemInfoEx(
        SystemExtendedHandleInformation,
        &returnLength,
        supHeapAlloc,
        supHeapFree);

    if (handleDump == NULL)
        return NULL;

    stBufferSize = sizeof(SUP_HANDLE_DUMP) +
        handleDump->NumberOfHandles * sizeof(PSUP_HANDLE_DUMP_ENTRY);

    resultSnapshot = (PSUP_HANDLE_DUMP)supHeapAlloc(stBufferSize);

    if (resultSnapshot) {
        for (i = 0; i < handleDump->NumberOfHandles; i++) {
            if (handleDump->Handles[i].UniqueProcessId == FilterUniqueProcessId) {
                resultSnapshot->Handles[cLast].Object = handleDump->Handles[i].Object;
                resultSnapshot->Handles[cLast].HandleValue = handleDump->Handles[i].HandleValue;
                cLast++;
            }
        }

        resultSnapshot->NumberOfHandles = cLast;

        RtlQuickSort((PVOID)&resultSnapshot->Handles,
            resultSnapshot->NumberOfHandles,
            sizeof(SUP_HANDLE_DUMP_ENTRY),
            (fObject) ? supxHandlesLookupCallback2 : supxHandlesLookupCallback);
    }

    supHeapFree(handleDump);

    return resultSnapshot;
}

/*
* supHandlesQueryObjectAddress
*
* Purpose:
*
* Find object address for given handle.
*
*/
BOOL supHandlesQueryObjectAddress(
    _In_ PSUP_HANDLE_DUMP SortedHandleList,
    _In_ HANDLE ObjectHandle,
    _Out_ PULONG_PTR ObjectAddress
)
{
    SUP_HANDLE_DUMP_ENTRY* SearchResult, SearchEntry;

    SearchEntry.HandleValue = (ULONG_PTR)ObjectHandle;

    SearchResult = (PSUP_HANDLE_DUMP_ENTRY)supBSearch(
        (PCVOID)&SearchEntry,
        SortedHandleList->Handles,
        SortedHandleList->NumberOfHandles,
        sizeof(SUP_HANDLE_DUMP_ENTRY),
        supxHandlesLookupCallback);

    if (SearchResult) {
        *ObjectAddress = (ULONG_PTR)SearchResult->Object;
        return TRUE;
    }

    *ObjectAddress = 0;
    return FALSE;
}

/*
* supPHLGetEntry
*
* Purpose:
*
* Return handle from handle list by process id.
*
*/
HANDLE supPHLGetEntry(
    _In_ PLIST_ENTRY ListHead,
    _In_ HANDLE UniqueProcessId
)
{
    PLIST_ENTRY Next, Head = ListHead;
    PHL_ENTRY* Item;

    if (!IsListEmpty(Head)) {
        Next = Head->Flink;
        while ((Next != NULL) && (Next != Head)) {
            Item = CONTAINING_RECORD(Next, PHL_ENTRY, ListEntry);
            if (Item->UniqueProcessId == UniqueProcessId)
                return Item->ProcessHandle;
            Next = Next->Flink;
        }
    }
    return NULL;
}

/*
* supPHLFree
*
* Purpose:
*
* Free list of handles.
*
*/
VOID supPHLFree(
    _In_ PLIST_ENTRY ListHead,
    _In_ BOOLEAN fClose
)
{
    PLIST_ENTRY Entry, NextEntry;
    PHL_ENTRY* Item;

    if (IsListEmpty(ListHead))
        return;

    for (Entry = ListHead->Flink, NextEntry = Entry->Flink;
        Entry != ListHead;
        Entry = NextEntry, NextEntry = Entry->Flink)
    {
        Item = CONTAINING_RECORD(Entry, PHL_ENTRY, ListEntry);
        RemoveEntryList(Entry);
        if (fClose) {
            if (Item->ProcessHandle)
                NtClose(Item->ProcessHandle);
        }
        supHeapFree(Item);
    }

}

/*
* supPHLCreate
*
* Purpose:
*
* Create simple handle list of running processes.
*
*/
BOOL supPHLCreate(
    _Inout_ PLIST_ENTRY ListHead,
    _In_ PBYTE ProcessList,
    _Out_ PULONG NumberOfProcesses,
    _Out_ PULONG NumberOfThreads
)
{
    ULONG NextEntryDelta = 0;
    ULONG numberOfThreads = 0, numberOfProcesses = 0;
    PHL_ENTRY* PsListItem;
    union {
        PSYSTEM_PROCESSES_INFORMATION ProcessEntry;
        PBYTE ListRef;
    } List;

    List.ListRef = ProcessList;

    do {

        List.ListRef += NextEntryDelta;

        numberOfThreads += List.ProcessEntry->ThreadCount;
        numberOfProcesses += 1;
        NextEntryDelta = List.ProcessEntry->NextEntryDelta;

        PsListItem = (PHL_ENTRY*)supHeapAlloc(sizeof(PHL_ENTRY));
        if (PsListItem) {

            PsListItem->UniqueProcessId = List.ProcessEntry->UniqueProcessId;
            PsListItem->DataPtr = (PVOID)List.ProcessEntry;

            if (List.ProcessEntry->ThreadCount) {

                supOpenProcess(
                    List.ProcessEntry->UniqueProcessId,
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    &PsListItem->ProcessHandle);

            }

            InsertHeadList(ListHead, &PsListItem->ListEntry);

        }

    } while (NextEntryDelta);

    *NumberOfThreads = numberOfThreads;
    *NumberOfProcesses = numberOfProcesses;

    return ((numberOfProcesses > 0) && (numberOfThreads > 0));
}

/*
* supxEnumerateSLCacheValueDescriptors
*
* Purpose:
*
* Walk each SL cache value descriptor entry, validate it and run optional callback.
*
*/
NTSTATUS supxEnumerateSLCacheValueDescriptors(
    _In_ SL_KMEM_CACHE * Cache,
    _In_opt_ PENUMERATE_SL_CACHE_VALUE_DESCRIPTORS_CALLBACK Callback,
    _In_opt_ PVOID Context
)
{
    ULONG_PTR CurrentPosition, MaxPosition;
    SL_KMEM_CACHE_VALUE_DESCRIPTOR* CacheDescriptor;

    __try {

        if (Cache->TotalSize < sizeof(SL_KMEM_CACHE))
            return STATUS_INVALID_PARAMETER;

        if (Cache->Version != 1)
            return STATUS_INVALID_PARAMETER;

        MaxPosition = (ULONG_PTR)RtlOffsetToPointer(Cache, Cache->TotalSize);
        if (MaxPosition < (ULONG_PTR)Cache)
            return STATUS_INVALID_PARAMETER;

        CacheDescriptor = (SL_KMEM_CACHE_VALUE_DESCRIPTOR*)&Cache->Descriptors;
        CurrentPosition = (ULONG_PTR)CacheDescriptor;
        MaxPosition = (ULONG_PTR)RtlOffsetToPointer(CacheDescriptor, Cache->SizeOfData);

    }
    __except (WOBJ_EXCEPTION_FILTER_LOG) {
        return GetExceptionCode();
    }

    do {
        __try {
            if ((CacheDescriptor->NameLength >= CacheDescriptor->Size) ||
                (CacheDescriptor->DataLength >= CacheDescriptor->Size))
            {
                return STATUS_INTERNAL_ERROR;
            }
        }
        __except (WOBJ_EXCEPTION_FILTER_LOG) {
            return GetExceptionCode();
        }

        if (Callback) {
            if (Callback(CacheDescriptor, Context))
                break;
        }

        __try {

            CurrentPosition += CacheDescriptor->Size;
            if (CurrentPosition >= MaxPosition)
                break;

            CacheDescriptor = (SL_KMEM_CACHE_VALUE_DESCRIPTOR*)RtlOffsetToPointer(CacheDescriptor, CacheDescriptor->Size);
        }
        __except (WOBJ_EXCEPTION_FILTER_LOG) {
            return GetExceptionCode();
        }

    } while (TRUE);

    return STATUS_SUCCESS;
}

/*
* supSLCacheRead
*
* Purpose:
*
* Read software licensing cache.
*
* N.B.
*
* Use supHeapFree to release allocated memory.
*
*/
PVOID supSLCacheRead(
    VOID)
{
    NTSTATUS Status;
    ULONG DataLength = 0;
    PVOID ReturnData = NULL;
    HANDLE KeyHandle = NULL;
    UNICODE_STRING ProductPolicyValue = RTL_CONSTANT_STRING(L"ProductPolicy");
    UNICODE_STRING ProductOptionsKey = RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\CurrentControlSet\\Control\\ProductOptions");
    OBJECT_ATTRIBUTES ObjectAttributes;

    KEY_VALUE_PARTIAL_INFORMATION* PolicyData;

    __try {

        InitializeObjectAttributes(&ObjectAttributes, &ProductOptionsKey, OBJ_CASE_INSENSITIVE, NULL, NULL);
        Status = NtOpenKey(&KeyHandle, KEY_READ, &ObjectAttributes);
        if (!NT_SUCCESS(Status))
            return NULL;

        Status = NtQueryValueKey(KeyHandle, &ProductPolicyValue,
            KeyValuePartialInformation, NULL, 0, &DataLength);

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            PolicyData = (KEY_VALUE_PARTIAL_INFORMATION*)supHeapAlloc(DataLength);
            if (PolicyData) {

                Status = NtQueryValueKey(KeyHandle, &ProductPolicyValue,
                    KeyValuePartialInformation, PolicyData, DataLength, &DataLength);

                if (NT_SUCCESS(Status) && (PolicyData->Type == REG_BINARY)) {
                    ReturnData = PolicyData;
                }
                else {
                    supHeapFree(PolicyData);
                }
            }
        }
        NtClose(KeyHandle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }

    return ReturnData;
}

/*
* supSLCacheEnumerate
*
* Purpose:
*
* Enumerate SL value descriptors and run optional callback.
*
*/
BOOLEAN supSLCacheEnumerate(
    _In_ PVOID CacheData,
    _In_opt_ PENUMERATE_SL_CACHE_VALUE_DESCRIPTORS_CALLBACK Callback,
    _In_opt_ PVOID Context)
{
    SL_KMEM_CACHE* Cache;

    Cache = (SL_KMEM_CACHE*)((KEY_VALUE_PARTIAL_INFORMATION*)(CacheData))->Data;

    return NT_SUCCESS(supxEnumerateSLCacheValueDescriptors(
        Cache,
        Callback,
        Context));
}

/*
* supxGetShellViewForDesktop
*
* Purpose:
*
* Use the shell view for the desktop using the shell windows automation to find the
* desktop web browser and then grabs its view.
*
* N.B. Taken entirely from Windows SDK sample.
*
*/
HRESULT supxGetShellViewForDesktop(
    REFIID riid,
    void** ppv
)
{
    IShellWindows* psw;
    HRESULT hr;
    HWND hwnd;
    IDispatch* pdisp;
    IShellBrowser* psb;
    VARIANT vtEmpty;
    IShellView* psv;

    *ppv = NULL;

#ifdef __cplusplus

    hr = CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&psw));
    if (SUCCEEDED(hr))
    {
        VariantInit(&vtEmpty);

        if (S_OK == psw->FindWindowSW(&vtEmpty, &vtEmpty, SWC_DESKTOP, (long*)(LONG_PTR)&hwnd, SWFO_NEEDDISPATCH, &pdisp))
        {
            hr = IUnknown_QueryService(pdisp, SID_STopLevelBrowser, IID_PPV_ARGS(&psb));
            if (SUCCEEDED(hr))
            {

                hr = psb->QueryActiveShellView(&psv);
                if (SUCCEEDED(hr))
                {
                    hr = psv->QueryInterface(riid, ppv);
                    psv->Release();
                }
                psb->Release();
            }
            pdisp->Release();
        }
        else
        {
            hr = E_FAIL;
        }
        psw->Release();
    }

#else

    VariantInit(&vtEmpty);

    hr = CoCreateInstance(&CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, &IID_IShellWindows, &psw);
    if (SUCCEEDED(hr))
    {
        if (S_OK == psw->lpVtbl->FindWindowSW(psw, &vtEmpty, &vtEmpty, SWC_DESKTOP, (long*)(LONG_PTR)&hwnd, SWFO_NEEDDISPATCH, &pdisp))
        {
            hr = IUnknown_QueryService((IUnknown*)pdisp, &SID_STopLevelBrowser, &IID_IShellBrowser, &psb);
            if (SUCCEEDED(hr))
            {
                hr = psb->lpVtbl->QueryActiveShellView(psb, &psv);
                if (SUCCEEDED(hr))
                {
                    hr = psv->lpVtbl->QueryInterface(psv, riid, ppv);
                    psv->lpVtbl->Release(psv);
                }
                psb->lpVtbl->Release(psb);
            }
            pdisp->lpVtbl->Release(pdisp);
        }
        else
        {
            hr = E_FAIL;
        }
        psw->lpVtbl->Release(psw);
    }

#endif
    return hr;
}

/*
* supxGetShellDispatchFromView
*
* Purpose:
*
* From a shell view object gets its automation interface and from that gets the shell
* application object that implements IShellDispatch2 and related interfaces.
*
* N.B. Taken entirely from Windows SDK sample.
*
*/
HRESULT supxGetShellDispatchFromView(IShellView * psv, REFIID riid, void** ppv)
{
    HRESULT hr;
    IDispatch* pdispBackground;
    IShellFolderViewDual* psfvd;
    IDispatch* pdisp;

    *ppv = NULL;

#ifdef __cplusplus

    hr = psv->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&pdispBackground));
    if (SUCCEEDED(hr))
    {
        hr = pdispBackground->QueryInterface(IID_PPV_ARGS(&psfvd));
        if (SUCCEEDED(hr))
        {
            hr = psfvd->get_Application(&pdisp);
            if (SUCCEEDED(hr))
            {
                hr = pdisp->QueryInterface(riid, ppv);
                pdisp->Release();
            }
            psfvd->Release();
        }
        pdispBackground->Release();
    }

#else

    hr = psv->lpVtbl->GetItemObject(psv, SVGIO_BACKGROUND, &IID_IDispatch, &pdispBackground);
    if (SUCCEEDED(hr))
    {
        hr = pdispBackground->lpVtbl->QueryInterface(pdispBackground, &IID_IShellFolderViewDual, &psfvd);
        if (SUCCEEDED(hr))
        {
            hr = psfvd->lpVtbl->get_Application(psfvd, &pdisp);
            if (SUCCEEDED(hr))
            {
                hr = pdisp->lpVtbl->QueryInterface(pdisp, riid, ppv);
                pdisp->lpVtbl->Release(pdisp);
            }
            psfvd->lpVtbl->Release(psfvd);
        }
        pdispBackground->lpVtbl->Release(pdispBackground);
    }

#endif
    return hr;
}

/*
* supShellExecInExplorerProcessEx
*
* Purpose:
*
* Run ShellExecute from Windows Explorer process through shell interfaces
* making it run with IL of Windows Explorer and not WinObjEx64.
*
*/
HRESULT supShellExecInExplorerProcessEx(
    _In_ PCWSTR pszFile,
    _In_opt_ PCWSTR pszArguments
)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IShellView* psv;
    IShellDispatch2* psd;
    BSTR bstrFile, bstrArgs = NULL;
    VARIANT vtEmpty, vtArgs;

    if (SUCCEEDED(hr)) {

#ifdef __cplusplus

        hr = supxGetShellViewForDesktop(IID_PPV_ARGS(&psv));
        if (SUCCEEDED(hr))
        {
            hr = supxGetShellDispatchFromView(psv, IID_PPV_ARGS(&psd));
            if (SUCCEEDED(hr))
            {
                bstrFile = SysAllocString(pszFile);
                hr = bstrFile ? S_OK : E_OUTOFMEMORY;
                if (SUCCEEDED(hr))
                {
                    VariantInit(&vtArgs);
                    VariantInit(&vtEmpty);

                    if (pszArguments) {
                        bstrArgs = SysAllocString(pszArguments);
                        hr = bstrArgs ? S_OK : E_OUTOFMEMORY;

                        if (SUCCEEDED(hr)) {
                            vtArgs.vt = VT_BSTR;
                            vtArgs.bstrVal = bstrArgs;

                            hr = psd->ShellExecuteW(bstrFile,
                                vtArgs, vtEmpty, vtEmpty, vtEmpty);

                            SysFreeString(bstrFile);
                        }
                    }
                    else {

                        hr = psd->ShellExecuteW(bstrFile,
                            vtEmpty, vtEmpty, vtEmpty, vtEmpty);

                    }

                }
                psd->Release();
            }
            psv->Release();
        }

#else
        hr = supxGetShellViewForDesktop(&IID_IShellView, &psv);
        if (SUCCEEDED(hr)) {
            hr = supxGetShellDispatchFromView(psv, &IID_IShellDispatch2, &psd);
            if (SUCCEEDED(hr))
            {
                bstrFile = SysAllocString(pszFile);
                hr = bstrFile ? S_OK : E_OUTOFMEMORY;
                if (SUCCEEDED(hr))
                {
                    VariantInit(&vtArgs);
                    VariantInit(&vtEmpty);

                    if (pszArguments) {

                        bstrArgs = SysAllocString(pszArguments);
                        hr = bstrArgs ? S_OK : E_OUTOFMEMORY;

                        if (SUCCEEDED(hr)) {
                            vtArgs.vt = VT_BSTR;
                            vtArgs.bstrVal = bstrArgs;

                            hr = psd->lpVtbl->ShellExecuteW(psd, bstrFile,
                                vtArgs, vtEmpty, vtEmpty, vtEmpty);

                            SysFreeString(bstrArgs);
                        }
                    }
                    else {

                        hr = psd->lpVtbl->ShellExecuteW(psd, bstrFile,
                            vtEmpty, vtEmpty, vtEmpty, vtEmpty);

                    }

                    SysFreeString(bstrFile);
                }

                psd->lpVtbl->Release(psd);
            }
            psv->lpVtbl->Release(psv);
        }
#endif
        CoUninitialize();
    }
    return hr;
}

/*
* supShellExecInExplorerProcess
*
* Purpose:
*
* Run ShellExecute from Windows Explorer process through shell interfaces
* making it run with IL of Windows Explorer and not WinObjEx64.
*
*/
HRESULT WINAPI supShellExecInExplorerProcess(
    _In_ PCWSTR pszFile)
{
    return supShellExecInExplorerProcessEx(pszFile, NULL);
}

/*
* supLoadIconForObjectType
*
* Purpose:
*
* Load icon for object (or its type) which properties is currently viewed.
*
*/
BOOLEAN supLoadIconForObjectType(
    _In_ HWND hwndDlg,
    _In_ PROP_OBJECT_INFO * Context,
    _In_ HIMAGELIST ImageList,
    _In_ BOOLEAN IsShadow)
{
    HICON hIcon;
    INT ImageIndex;

    if (IsShadow)
        ImageIndex = Context->ShadowTypeDescription->ImageIndex;
    else
        ImageIndex = Context->TypeDescription->ImageIndex;

    hIcon = ImageList_GetIcon(
        ImageList,
        ImageIndex,
        ILD_NORMAL | ILD_TRANSPARENT);

    if (hIcon) {

        SendDlgItemMessage(hwndDlg, ID_OBJECT_ICON,
            STM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

        if (IsShadow)
            Context->ObjectTypeIcon = hIcon;
        else
            Context->ObjectIcon = hIcon;

        return TRUE;
    }

    return FALSE;
}

/*
* supDestroyIconForObjectType
*
* Purpose:
*
* Destroy icon used to represent object (or its type) which properties is currently viewed.
*
*/
VOID supDestroyIconForObjectType(
    _In_ PROP_OBJECT_INFO * Context
)
{
    if (Context->IsType) {
        if (Context->ObjectTypeIcon) {
            DestroyIcon(Context->ObjectTypeIcon);
            Context->ObjectTypeIcon = NULL;
        }
    }
    if (Context->ObjectIcon) {
        DestroyIcon(Context->ObjectIcon);
        Context->ObjectIcon = NULL;
    }
}

/*
* supxDeleteKeyRecursive
*
* Purpose:
*
* Delete key and all it subkeys/values.
*
*/
BOOL supxDeleteKeyRecursive(
    _In_ HKEY hKeyRoot,
    _In_ LPWSTR lpSubKey)
{
    LPWSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    WCHAR szName[MAX_PATH + 1];
    HKEY hKey;
    FILETIME ftWrite;

    //
    // Attempt to delete key as is.
    //
    lResult = RegDeleteKey(hKeyRoot, lpSubKey);
    if (lResult == ERROR_SUCCESS)
        return TRUE;

    //
    // Try to open key to check if it exist.
    //
    lResult = RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);
    if (lResult != ERROR_SUCCESS) {
        if (lResult == ERROR_FILE_NOT_FOUND)
            return TRUE;
        else
            return FALSE;
    }

    //
    // Add slash to the key path if not present.
    //
    lpEnd = _strend(lpSubKey);
    if (*(lpEnd - 1) != TEXT('\\')) {
        *lpEnd++ = TEXT('\\');
        *lpEnd = TEXT('\0');
    }

    //
    // Enumerate subkeys and call this func for each.
    //
    dwSize = MAX_PATH;
    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
        NULL, NULL, &ftWrite);

    if (lResult == ERROR_SUCCESS) {

        do {

            _strncpy(lpEnd, MAX_PATH, szName, MAX_PATH);

            if (!supxDeleteKeyRecursive(hKeyRoot, lpSubKey))
                break;

            dwSize = MAX_PATH;

            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                NULL, NULL, &ftWrite);

        } while (lResult == ERROR_SUCCESS);
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey(hKey);

    //
    // Delete current key, all it subkeys should be already removed.
    //
    lResult = RegDeleteKey(hKeyRoot, lpSubKey);
    if (lResult == ERROR_SUCCESS)
        return TRUE;

    return FALSE;
}

/*
* supRegDeleteKeyRecursive
*
* Purpose:
*
* Delete key and all it subkeys/values.
*
* Remark:
*
* SubKey should not be longer than 260 chars.
*
*/
BOOL supRegDeleteKeyRecursive(
    _In_ HKEY hKeyRoot,
    _In_ LPCWSTR lpSubKey)
{
    WCHAR szKeyName[MAX_PATH * 2];
    RtlSecureZeroMemory(szKeyName, sizeof(szKeyName));
    _strncpy(szKeyName, MAX_PATH * 2, lpSubKey, MAX_PATH);
    return supxDeleteKeyRecursive(hKeyRoot, szKeyName);
}

/*
* supHashString
*
* Purpose:
*
* Create sdbm hash for given string.
*
* N.B. Case sensitive.
*
*/
ULONG supHashString(
    _In_ PCWSTR String,
    _In_ ULONG Length)
{
    ULONG hashValue = 0, nChars = Length;
    PCWSTR stringBuffer = String;

    while (nChars-- != 0)
        hashValue = (hashValue * 65599) + *stringBuffer++;

    return hashValue;
}

/*
* supHashStringAnsi
*
* Purpose:
*
* Create sdbm hash for given string.
*
* N.B. Case sensitive.
*
*/
ULONG supHashStringAnsi(
    _In_ PCSTR String,
    _In_ ULONG Length)
{
    ULONG hashValue = 0, nChars = Length;
    PCSTR stringBuffer = String;

    while (nChars-- != 0)
        hashValue = (hashValue * 65599) + *stringBuffer++;

    return hashValue;
}

/*
* supHashUnicodeString
*
* Purpose:
*
* Create sdbm hash for given UNICODE_STRING.
*
* N.B. Case sensitive.
*
*/
ULONG supHashUnicodeString(
    _In_ CONST UNICODE_STRING * String)
{
    return supHashString(String->Buffer, String->Length / sizeof(WCHAR));
}

/*
* supCreateSystemAdminAccessSD
*
* Purpose:
*
* Create security descriptor with Admin/System ACL set.
*
*/
NTSTATUS supCreateSystemAdminAccessSD(
    _Out_ PSECURITY_DESCRIPTOR* SecurityDescriptor,
    _Out_ PACL* DefaultAcl
)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    ULONG aclSize = 0;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PACL pAcl = NULL;
    PSECURITY_DESCRIPTOR securityDescriptor = NULL;

    UCHAR sidBuffer[2 * sizeof(SID)];

    *SecurityDescriptor = NULL;
    *DefaultAcl = NULL;

    do {

        RtlSecureZeroMemory(sidBuffer, sizeof(sidBuffer));

        securityDescriptor = (PSECURITY_DESCRIPTOR)supHeapAlloc(sizeof(SECURITY_DESCRIPTOR));
        if (securityDescriptor == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        aclSize += RtlLengthRequiredSid(1); //LocalSystem sid
        aclSize += RtlLengthRequiredSid(2); //Admin group sid
        aclSize += sizeof(ACL);
        aclSize += 2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(ULONG));

        pAcl = (PACL)supHeapAlloc(aclSize);
        if (pAcl == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        ntStatus = RtlCreateAcl(pAcl, aclSize, ACL_REVISION);
        if (!NT_SUCCESS(ntStatus))
            break;

        //
        // Local System - Generic All.
        //
        RtlInitializeSid(sidBuffer, &ntAuthority, 1);
        *(RtlSubAuthoritySid(sidBuffer, 0)) = SECURITY_LOCAL_SYSTEM_RID;
        RtlAddAccessAllowedAce(pAcl, ACL_REVISION, GENERIC_ALL, (PSID)sidBuffer);

        //
        // Admins - Generic All.
        //
        RtlInitializeSid(sidBuffer, &ntAuthority, 2);
        *(RtlSubAuthoritySid(sidBuffer, 0)) = SECURITY_BUILTIN_DOMAIN_RID;
        *(RtlSubAuthoritySid(sidBuffer, 1)) = DOMAIN_ALIAS_RID_ADMINS;
        RtlAddAccessAllowedAce(pAcl, ACL_REVISION, GENERIC_ALL, (PSID)sidBuffer);

        ntStatus = RtlCreateSecurityDescriptor(securityDescriptor,
            SECURITY_DESCRIPTOR_REVISION1);
        if (!NT_SUCCESS(ntStatus))
            break;

        ntStatus = RtlSetDaclSecurityDescriptor(securityDescriptor,
            TRUE,
            pAcl,
            FALSE);

        if (!NT_SUCCESS(ntStatus))
            break;

        *SecurityDescriptor = securityDescriptor;
        *DefaultAcl = pAcl;

    } while (FALSE);

    if (!NT_SUCCESS(ntStatus)) {

        if (pAcl) supHeapFree(pAcl);

        if (securityDescriptor) {
            supHeapFree(securityDescriptor);
        }

        *SecurityDescriptor = NULL;
        *DefaultAcl = NULL;
    }

    return ntStatus;
}

/*
* supGetTimeAsSecondsSince1970
*
* Purpose:
*
* Return seconds since 1970.
*
*/
ULONG supGetTimeAsSecondsSince1970(
    VOID
)
{
    LARGE_INTEGER fileTime;
    ULONG seconds = 0;

    GetSystemTimeAsFileTime((PFILETIME)&fileTime);
    RtlTimeToSecondsSince1970(&fileTime, &seconds);
    return seconds;
}

/*
* supRichEdit32Load
*
* Purpose:
*
* Preload richedit32 library and classes.
*
*/
BOOL supRichEdit32Load()
{
    WCHAR szBuffer[MAX_PATH * 2];
    HMODULE hRichEdit;

    hRichEdit = GetModuleHandle(T_RICHEDIT_LIB);
    if (hRichEdit == NULL) {

        RtlSecureZeroMemory(&szBuffer, sizeof(szBuffer));

        RtlStringCchPrintfSecure(szBuffer,
            sizeof(szBuffer) / sizeof(szBuffer[0]),
            TEXT("%s\\%s"),
            g_WinObj.szSystemDirectory,
            T_RICHEDIT_LIB);

        hRichEdit = LoadLibraryEx(szBuffer, NULL, 0);
    }

    return (hRichEdit != NULL);
}

/*
* supReportAbnormalTermination
*
* Purpose:
*
* Log abnormal terminations from try/finally blocks.
*
*/
VOID supReportAbnormalTermination(
    _In_ LPCWSTR FunctionName
)
{
    WCHAR szBuffer[512];

    _strcpy(szBuffer, TEXT("AbnormalTermination of "));
    _strcat(szBuffer, FunctionName);

    logAdd(WOBJ_LOG_ENTRY_ERROR,
        szBuffer);
}

/*
* supReportException
*
* Purpose:
*
* Log details about exception.
*
*/
VOID supReportException(
    _In_ ULONG ExceptionCode,
    _In_opt_ PEXCEPTION_POINTERS ExceptionPointers
)
{
    WCHAR szBuffer[512];

    _strcpy(szBuffer, TEXT("Exception 0x"));
    ultohex(ExceptionCode, _strend(szBuffer));

    if (ExceptionPointers) {
        if (ExceptionCode == STATUS_ACCESS_VIOLATION) {
            switch (ExceptionPointers->ExceptionRecord->ExceptionInformation[0]) {
            case 0:
                _strcat(szBuffer, TEXT(", read at address: 0x"));
                break;
            case 1:
                _strcat(szBuffer, TEXT(", write at address: 0x"));
                break;
            }
            u64tohex(ExceptionPointers->ExceptionRecord->ExceptionInformation[1], _strend(szBuffer));
        }
    }

    logAdd(WOBJ_LOG_ENTRY_ERROR,
        szBuffer);
}

/*
* supReportAPIError
*
* Purpose:
*
* Log details about failed API call.
*
*/
VOID supReportAPIError(
    _In_ LPCWSTR FunctionName,
    _In_ NTSTATUS NtStatus
)
{
    WCHAR szBuffer[512];

    RtlStringCchPrintfSecure(szBuffer,
        512,
        TEXT("%ws 0x%lX"),
        FunctionName,
        NtStatus);

    logAdd(WOBJ_LOG_ENTRY_ERROR,
        szBuffer);
}

/*
* supIsFileImageSection
*
* Purpose:
*
* Return TRUE if section attributes include image and file flags.
*
*/
BOOLEAN supIsFileImageSection(
    _In_ ULONG AllocationAttributes)
{
    return ((AllocationAttributes & SEC_IMAGE) && (AllocationAttributes & SEC_FILE));
}

/*
* supIsDriverShimmed
*
* Purpose:
*
* Return TRUE if driver shimmed by KSE.
*
*/
BOOLEAN supIsDriverShimmed(
    _In_ PKSE_ENGINE_DUMP KseEngineDump,
    _In_ PVOID DriverBaseAddress)
{
    PLIST_ENTRY Entry, NextEntry, ListHead;
    KSE_SHIMMED_DRIVER* ShimmedDriver;


    if (KseEngineDump->Valid == FALSE)
        return FALSE;

    ListHead = &KseEngineDump->ShimmedDriversDumpListHead;

    ASSERT_LIST_ENTRY_VALID_BOOLEAN(ListHead);

    if (IsListEmpty(ListHead))
        return FALSE;

    for (Entry = ListHead->Flink, NextEntry = Entry->Flink;
        Entry != ListHead;
        Entry = NextEntry, NextEntry = Entry->Flink)
    {
        ShimmedDriver = CONTAINING_RECORD(Entry, KSE_SHIMMED_DRIVER, ListEntry);
        if (DriverBaseAddress == ShimmedDriver->DriverBaseAddress)
            return TRUE;
    }

    return FALSE;
}

size_t supxEscStrlen(wchar_t* s)
{
    size_t  result = 2;
    wchar_t* s0 = s;

    while (*s)
    {
        if (*s == L'"')
            ++result;
        ++s;
    }

    return result + (s - s0);
}

wchar_t* supxEscStrcpy(wchar_t* dst, wchar_t* src)
{
    *(dst++) = L'"';

    while ((*dst = *src) != L'\0')
    {
        if (*src == L'"')
            *(++dst) = L'"';

        ++src;
        ++dst;
    }

    *(dst++) = L'"';
    *dst = L'\0';

    return dst;
}

/*
* supxListViewExportCSV
*
* Purpose:
*
* Export listview entries into file in csv format.
*
*/
BOOL supxListViewExportCSV(
    _In_ HWND List,
    _In_ PWCHAR FileName)
{
    HWND            hdr = ListView_GetHeader(List);
    int             pass, i, c, col_count = Header_GetItemCount(hdr), icount = 1 + ListView_GetItemCount(List);
    HDITEM          ih;
    LVITEM          lvi;
    PWCHAR          text, buffer0 = NULL, buffer = NULL;
    BOOL            result = FALSE;
    SIZE_T          total_lenght;
    DWORD           iobytes;
    HANDLE          f;

    text = (PWCHAR)supVirtualAlloc(32768 * sizeof(WCHAR));
    if (!text)
        return FALSE;

    RtlSecureZeroMemory(&ih, sizeof(HDITEM));
    RtlSecureZeroMemory(&lvi, sizeof(LVITEM));

    ih.pszText = lvi.pszText = text;
    ih.cchTextMax = lvi.cchTextMax = 32767;

    for (pass = 0; pass < 2; ++pass)
    {
        total_lenght = 0;

        for (i = 0; i < icount; ++i)
        {
            for (c = 0; c < col_count; ++c)
            {
                text[0] = L'\0';
                if (i == 0)
                {
                    ih.mask = HDI_TEXT | HDI_ORDER;
                    ih.iOrder = c;
                    Header_GetItem(hdr, c, &ih);
                }
                else
                {
                    lvi.mask = LVIF_TEXT;
                    lvi.iItem = i - 1;
                    lvi.iSubItem = c;
                    ListView_GetItem(List, &lvi);
                }
                total_lenght += supxEscStrlen(text) + 1;

                if (buffer)
                {
                    buffer = supxEscStrcpy(buffer, text);
                    if (c != col_count - 1)
                    {
                        *(buffer++) = L',';
                    }
                    else
                    {
                        *(buffer++) = L'\r';
                        *(buffer++) = L'\n';
                    }
                }
            }
            ++total_lenght;
        }

        if (buffer0 == NULL)
        {
            buffer0 = (PWCHAR)supVirtualAlloc((1 + total_lenght) * sizeof(WCHAR));
            if (!buffer0)
                break;
        }
        else
        {
            f = CreateFile(FileName,
                GENERIC_WRITE | SYNCHRONIZE,
                FILE_SHARE_READ,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

            if (f != INVALID_HANDLE_VALUE) {

                WriteFile(f, buffer0,
                    (DWORD)(total_lenght * sizeof(WCHAR)),
                    &iobytes, NULL);

                CloseHandle(f);
                result = TRUE;
            }
            supVirtualFree(buffer0);
        }
        buffer = buffer0;
    }

    supVirtualFree(text);
    return result;
}

/*
* supListViewExportToFile
*
* Purpose:
*
* Export listview contents to the specified file.
*
*/
BOOL supListViewExportToFile(
    _In_ LPCWSTR FileName,
    _In_ HWND WindowHandle,
    _In_ HWND ListView
)
{
    BOOL bResult = FALSE;
    WCHAR szExportFileName[MAX_PATH + 1];

    RtlSecureZeroMemory(&szExportFileName, sizeof(szExportFileName));

    _strcpy(szExportFileName, FileName);
    if (supSaveDialogExecute(WindowHandle,
        (LPWSTR)&szExportFileName,
        T_CSV_FILE_FILTER))
    {
        SetCapture(WindowHandle);
        supSetWaitCursor(TRUE);

        bResult = supxListViewExportCSV(ListView, szExportFileName);

        supSetWaitCursor(FALSE);
        ReleaseCapture();
    }

    return bResult;
}

/*
* supStatusBarSetText
*
* Purpose:
*
* Display status in status bar part.
*
*/
VOID supStatusBarSetText(
    _In_ HWND hwndStatusBar,
    _In_ WPARAM partIndex,
    _In_ LPCWSTR lpText
)
{
    SendMessage(hwndStatusBar, SB_SETTEXT, partIndex, (LPARAM)lpText);
}

/*
* supJumpToFileListView
*
* Purpose:
*
* Jump from listview to file on disk.
*
*/
VOID supJumpToFileListView(
    _In_ HWND hwndList,
    _In_ INT iFileNameColumn
)
{
    INT iPos;
    LPWSTR lpDriverName = NULL, lpConvertedName = NULL;
    SIZE_T sz;

    do {

        iPos = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED);
        if (iPos < 0)
            break;

        lpConvertedName = (LPWSTR)supHeapAlloc(UNICODE_STRING_MAX_CHARS);
        if (lpConvertedName == NULL)
            break;

        sz = 0;
        lpDriverName = supGetItemText(hwndList, iPos, iFileNameColumn, &sz);
        if (lpDriverName == NULL)
            break;

        if (supGetWin32FileName(
            lpDriverName,
            lpConvertedName,
            UNICODE_STRING_MAX_CHARS))
        {
            supJumpToFile(lpConvertedName);
        }

    } while (FALSE);

    if (lpDriverName) supHeapFree(lpDriverName);
    if (lpConvertedName) supHeapFree(lpConvertedName);
}

/*
* supQueryAlpcPortObjectTypeIndex
*
* Purpose:
*
* Create dummy WinObjEx ALPC port, remember it object type index and destroy port.
*
*/
VOID supQueryAlpcPortObjectTypeIndex(
    _In_ PVOID PortGlobal
)
{
    PWINOBJ_PORT_GLOBAL portGlobal = (PWINOBJ_PORT_GLOBAL)PortGlobal;
    NTSTATUS ntStatus;
    HANDLE portHandle = NULL;
    UNICODE_STRING portName = RTL_CONSTANT_STRING(L"\\Rpc Control\\WinObjEx64Port");
    OBJECT_ATTRIBUTES objectAttributes;
    PSYSTEM_HANDLE_INFORMATION_EX pHandles = NULL;

    ULONG sdLength;
    SID_IDENTIFIER_AUTHORITY WorldSidAuthority = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID SeWorldSid = NULL;
    PSID SeRestrictedSid = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pDacl = NULL;

    do {

        SeWorldSid = supHeapAlloc(RtlLengthRequiredSid(1));
        if (SeWorldSid == NULL)
            break;

        RtlInitializeSid(SeWorldSid, &WorldSidAuthority, 1);
        *(RtlSubAuthoritySid(SeWorldSid, 0)) = SECURITY_WORLD_RID;

        ntStatus = RtlAllocateAndInitializeSid(&NtAuthority,
            1,
            SECURITY_RESTRICTED_CODE_RID,
            0, 0, 0, 0, 0, 0, 0,
            &SeRestrictedSid);

        if (!NT_SUCCESS(ntStatus))
            break;

        sdLength = SECURITY_DESCRIPTOR_MIN_LENGTH +
            (ULONG)sizeof(ACL) +
            2 * (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
            RtlLengthSid(SeWorldSid) +
            RtlLengthSid(SeRestrictedSid) +
            8;

        pSD = supHeapAlloc(sdLength);
        if (pSD == NULL)
            break;

        pDacl = (PACL)((PCHAR)pSD + SECURITY_DESCRIPTOR_MIN_LENGTH);

        ntStatus = RtlCreateSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
        if (!NT_SUCCESS(ntStatus))
            break;

        ntStatus = RtlCreateAcl(pDacl, (ULONG)(sdLength - SECURITY_DESCRIPTOR_MIN_LENGTH), ACL_REVISION2);
        if (!NT_SUCCESS(ntStatus))
            break;

        ntStatus = RtlAddAccessAllowedAce(pDacl,
            ACL_REVISION2,
            PORT_ALL_ACCESS,
            SeWorldSid);

        if (!NT_SUCCESS(ntStatus))
            break;

        ntStatus = RtlAddAccessAllowedAce(pDacl,
            ACL_REVISION2,
            PORT_ALL_ACCESS,
            SeRestrictedSid);

        if (!NT_SUCCESS(ntStatus))
            break;

        ntStatus = RtlSetDaclSecurityDescriptor(pSD,
            TRUE,
            pDacl,
            FALSE);

        if (!NT_SUCCESS(ntStatus))
            break;

        InitializeObjectAttributes(&objectAttributes, &portName, OBJ_CASE_INSENSITIVE, NULL, pSD);

        ntStatus = NtCreatePort(&portHandle,
            &objectAttributes,
            0,
            sizeof(PORT_MESSAGE),
            0);

        if (NT_SUCCESS(ntStatus)) {

            pHandles = (PSYSTEM_HANDLE_INFORMATION_EX)supGetSystemInfo(SystemExtendedHandleInformation,
                NULL);
            if (pHandles) {

                //
                // Query ALPC port object type.
                //
                if (supQueryObjectFromHandleEx(pHandles,
                    portHandle,
                    NULL,
                    &portGlobal->AlpcPortTypeIndex))
                {
                    portGlobal->Initialized = TRUE;
                }

                supHeapFree(pHandles);
            }

            //
            // Destroy port object.
            //
            NtClose(portHandle);
        }

    } while (FALSE);

    if (SeWorldSid) supHeapFree(SeWorldSid);
    if (SeRestrictedSid) RtlFreeSid(SeRestrictedSid);
    if (pSD) supHeapFree(pSD);

}

/*
* supQueryProcessImageFileNameWin32
*
* Purpose:
*
* Query Win32 process filename.
*
*/
NTSTATUS supQueryProcessImageFileNameWin32(
    _In_ HANDLE UniqueProcessId,
    _Out_ PUNICODE_STRING* ProcessImageFileName
)
{
    NTSTATUS ntStatus;
    HANDLE hProcess = NULL;

    *ProcessImageFileName = NULL;

    ntStatus = supOpenProcess(UniqueProcessId,
        PROCESS_QUERY_LIMITED_INFORMATION,
        &hProcess);

    if (NT_SUCCESS(ntStatus)) {

        ntStatus = supQueryProcessInformation(hProcess,
            ProcessImageFileNameWin32,
            (PVOID*)ProcessImageFileName,
            NULL,
            (PNTSUPMEMALLOC)supHeapAlloc,
            (PNTSUPMEMFREE)supHeapFree);

        NtClose(hProcess);

    }

    return ntStatus;
}

/*
* supQueryProcessImageFileNameByProcessId
*
* Purpose:
*
* Query process filename by process id in native format.
*
*/
NTSTATUS supQueryProcessImageFileNameByProcessId(
    _In_ HANDLE UniqueProcessId,
    _Out_ PUNICODE_STRING ProcessImageFileName
)
{
    return ntsupQueryProcessImageFileNameByProcessId(UniqueProcessId,
        ProcessImageFileName,
        (PNTSUPMEMALLOC)supHeapAlloc,
        (PNTSUPMEMFREE)supHeapFree);
}

/*
* supGetSidFromAce
*
* Purpose:
*
* Return Sid associated with Ace.
*
*/
PSID supGetSidFromAce(
    _In_ PACE_HEADER AceHeader
)
{
    PACCESS_ALLOWED_OBJECT_ACE paoa = (PACCESS_ALLOWED_OBJECT_ACE)AceHeader;

    if (AceHeader->AceType >= ACCESS_MIN_MS_OBJECT_ACE_TYPE &&
        AceHeader->AceType <= ACCESS_MAX_MS_OBJECT_ACE_TYPE)
    {
        switch (paoa->Flags & (ACE_OBJECT_TYPE_PRESENT | ACE_INHERITED_OBJECT_TYPE_PRESENT)) {
        case 0:
            return &((PACCESS_ALLOWED_OBJECT_ACE)AceHeader)->ObjectType;
        case ACE_OBJECT_TYPE_PRESENT:
        case ACE_INHERITED_OBJECT_TYPE_PRESENT:
            return &((PACCESS_ALLOWED_OBJECT_ACE)AceHeader)->InheritedObjectType;
        default:
            return &((PACCESS_ALLOWED_OBJECT_ACE)AceHeader)->SidStart;
        }
    }

    return &((PACCESS_ALLOWED_ACE)AceHeader)->SidStart;
}

/*
* supQuerySecurityInformation
*
* Purpose:
*
* Query object security information with variable size.
*
* Returned buffer must be freed with supHeapFree after usage.
*
*/
NTSTATUS supQuerySecurityInformation(
    _In_ HANDLE ObjectHandle,
    _In_ SECURITY_INFORMATION SecurityInformationClass,
    _Out_ PVOID* Buffer,
    _Out_opt_ PULONG ReturnLength)
{
    return ntsupQuerySystemObjectInformationVariableSize(
        (PFN_NTQUERYROUTINE)NtQuerySecurityObject,
        ObjectHandle,
        (DWORD)SecurityInformationClass,
        Buffer,
        ReturnLength,
        (PNTSUPMEMALLOC)supHeapAlloc,
        (PNTSUPMEMFREE)supHeapFree);
}

/*
* supHandleContextMenuMsgForListView
*
* Purpose:
*
* WM_CONTEXT handler for dialogs with a listview.
*
*/
VOID supHandleContextMenuMsgForListView(
    _In_ HWND hwndDlg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ HWND hwndControl,
    _In_ pfnPopupMenuHandler MenuHandler,
    _In_opt_ PVOID lpUserParam
)
{
    INT mark;
    RECT crc;

    if ((HWND)wParam == hwndControl) {

        mark = ListView_GetSelectionMark(hwndControl);

        RtlSecureZeroMemory(&crc, sizeof(crc));
        if (lParam == MAKELPARAM(-1, -1)) {
            ListView_GetItemRect(hwndControl, mark, &crc, TRUE);
            crc.top = crc.bottom;
            ClientToScreen(hwndControl, (LPPOINT)&crc);
        }
        else
            GetCursorPos((LPPOINT)&crc);

        MenuHandler(hwndDlg, (LPPOINT)&crc, lpUserParam);

    }
}

/*
* supAddLVColumnsFromArray
*
* Purpose:
*
* Add columns from array to the listview.
*
*/
ULONG supAddLVColumnsFromArray(
    _In_ HWND ListView,
    _In_ PLVCOLUMNS_DATA ColumnsData,
    _In_ ULONG NumberOfColumns
)
{
    ULONG iColumn;

    for (iColumn = 0; iColumn < NumberOfColumns; iColumn++) {

        if (-1 == supAddListViewColumn(ListView,
            iColumn,
            iColumn,
            iColumn,
            ColumnsData[iColumn].ImageIndex,
            ColumnsData[iColumn].Format,
            ColumnsData[iColumn].Name,
            ColumnsData[iColumn].Width))
        {
            break;
        }
    }

    return iColumn;
}

/*
* supShowInitError
*
* Purpose:
*
* Display initialization error depending on it type.
*
*/
VOID supShowInitError(
    _In_ DWORD ErrorType
)
{
    WCHAR szErrorBuffer[MAX_PATH * 2];
    LPWSTR lpError;

    //
    // CRT not initialized, no fancy swprinfs for you.
    //
    if (ErrorType == INIT_ERROR_NOCRT) {

        MessageBox(GetDesktopWindow(),
            (LPCWSTR)T_WOBJINIT_NOCRT,
            (LPCWSTR)PROGRAM_NAME,
            MB_ICONWARNING | MB_OK);

        return;
    }

    switch (ErrorType) {

    case INIT_ERROR_NOHEAP:
        lpError = L"Heap not allocated";
        break;

    case INIT_ERROR_NOTEMP:
        lpError = L"%temp% not resolved";
        break;

    case INIT_ERROR_NOWINDIR:
        lpError = L"Windows directory not resolved";
        break;

    case INIT_ERROR_NOSYS32DIR:
        lpError = L"System32 directory not resolved";
        break;

    case INIT_ERROR_NOPROGDIR:
        lpError = L"Program directory not resolved";
        break;

    case INIT_ERROR_NOCLASS:
        lpError = L"Main window class not registered";
        break;

    case INIT_ERROR_NOMAINWND:
        lpError = L"Main window not created";
        break;

    case INIT_ERROR_NOICCX:
        lpError = L"Common Controls Library";
        break;

    case INIT_ERROR_NOLISTWND:
        lpError = L"Main list window not created";
        break;

    case INIT_ERROR_NOTREEWND:
        lpError = L"Main tree window not created";
        break;

    case INIT_ERROR_NOTLBARWND:
        lpError = L"Main toolbar window not created";
        break;

    default:
        lpError = L"Unknown initialization error";
        break;
    }

    RtlStringCchPrintfSecure(szErrorBuffer,
        MAX_PATH * 2,
        TEXT("WinObjEx64 failed to initialize: %ws, abort"),
        lpError);

    MessageBox(GetDesktopWindow(),
        (LPWSTR)szErrorBuffer,
        (LPCWSTR)PROGRAM_NAME,
        MB_ICONWARNING | MB_OK);

}

/*
* supExtractFileName
*
* Purpose:
*
* Return filename part from given path.
*
*/
wchar_t* supExtractFileName(
    _In_ const wchar_t* lpFullPath
)
{
    wchar_t* p = (wchar_t*)lpFullPath;

    if (lpFullPath == 0)
        return 0;

    while (*lpFullPath != (wchar_t)0) {
        if (*lpFullPath == (wchar_t)'\\')
            p = (wchar_t*)lpFullPath + 1;
        lpFullPath++;
    }
    return p;
}

/*
* supObjectDumpHandlePopupMenu
*
* Purpose:
*
* Object dump popup construction
*
*/
VOID supObjectDumpHandlePopupMenu(
    _In_ HWND hwndDlg,
    _In_ HWND hwndTreeList,
    _In_ INT *pSubItemHit,
    _In_ LPARAM lParam
)
{
    POINT pt1;
    HMENU hMenu;

    if (GetCursorPos(&pt1) == FALSE)
        return;

    hMenu = CreatePopupMenu();
    if (hMenu) {

        if (supTreeListAddCopyValueItem(hMenu,
            hwndTreeList,
            ID_OBJECT_COPY,
            0,
            lParam,
            pSubItemHit)) 
        {
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt1.x, pt1.y, 0, hwndDlg, NULL);
        }
        DestroyMenu(hMenu);
    }
}

/*
* supObDumpShowError
*
* Purpose:
*
* Hide all windows for given hwnd and display error text with custom text if specified.
*
*/
VOID supObDumpShowError(
    _In_ HWND hwndDlg,
    _In_opt_ LPWSTR lpMessageText
)
{
    ENUMCHILDWNDDATA ChildWndData;

    if (GetWindowRect(hwndDlg, &ChildWndData.Rect)) {
        ChildWndData.nCmdShow = SW_HIDE;
        EnumChildWindows(hwndDlg, supCallbackShowChildWindow, (LPARAM)&ChildWndData);
    }

    if (lpMessageText) {
        SetDlgItemText(hwndDlg, ID_OBJECTDUMPERROR, lpMessageText);
    }

    ShowWindow(GetDlgItem(hwndDlg, ID_OBJECTDUMPERROR), SW_SHOW);
}

/*
* supGetFirmwareType
*
* Purpose:
*
* Return firmware type.
*
*/
NTSTATUS supGetFirmwareType(
    _Out_ PFIRMWARE_TYPE FirmwareType
)
{
    NTSTATUS ntStatus;
    ULONG returnLength = 0;
    SYSTEM_BOOT_ENVIRONMENT_INFORMATION sbei;

    *FirmwareType = FirmwareTypeUnknown;

    RtlSecureZeroMemory(&sbei, sizeof(sbei));

    ntStatus = NtQuerySystemInformation(SystemBootEnvironmentInformation,
        &sbei,
        sizeof(sbei),
        &returnLength);

    if (NT_SUCCESS(ntStatus)) {
        *FirmwareType = sbei.FirmwareType;

    }

    return ntStatus;
}

/*
* supIsBootDriveVHD
*
* Purpose:
*
* Query if the current boot drive is VHD type.
*
*/
NTSTATUS supIsBootDriveVHD(
    _Out_ PBOOLEAN IsVHD
)
{
    NTSTATUS ntStatus;
    ULONG returnLength = 0;
    SYSTEM_VHD_BOOT_INFORMATION* psvbi;

    *IsVHD = FALSE;

    psvbi = (SYSTEM_VHD_BOOT_INFORMATION*)supHeapAlloc(PAGE_SIZE);
    if (psvbi) {
        ntStatus = NtQuerySystemInformation(SystemVhdBootInformation, psvbi, PAGE_SIZE, &returnLength);
        if (NT_SUCCESS(ntStatus)) {
            *IsVHD = psvbi->OsDiskIsVhd;
        }
        supHeapFree(psvbi);
    }
    else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

/*
* supPathAddBackSlash
*
* Purpose:
*
* Add trailing backslash to the path if it doesn't have one.
*
*/
LPWSTR supPathAddBackSlash(
    _In_ LPWSTR lpszPath
)
{
    SIZE_T nLength;
    LPWSTR lpszEnd, lpszPrev, lpszResult = NULL;

    nLength = _strlen(lpszPath);

    if (nLength) {

        lpszEnd = lpszPath + nLength;

        if (lpszPath == lpszEnd)
            lpszPrev = lpszPath;
        else
            lpszPrev = (LPWSTR)lpszEnd - 1;

        if (*lpszPrev != TEXT('\\')) {
            *lpszEnd++ = TEXT('\\');
            *lpszEnd = TEXT('\0');
        }

        lpszResult = lpszEnd;

    }

    return lpszResult;
}

__inline WCHAR nibbletoh(BYTE c, BOOLEAN upcase)
{
    if (c < 10)
        return L'0' + c;

    c -= 10;

    if (upcase)
        return L'A' + c;

    return L'a' + c;
}

/*
* supPrintHash
*
* Purpose:
*
* Output hash.
* Returned buffer must be freed with supHeapFree when no longer needed.
*
*/
LPWSTR supPrintHash(
    _In_reads_bytes_(Length) LPBYTE Buffer,
    _In_ ULONG Length,
    _In_ BOOLEAN UpcaseHex
)
{
    ULONG   c;
    PWCHAR  lpText;
    BYTE    x;

    lpText = (LPWSTR)supHeapAlloc(sizeof(WCHAR) + ((SIZE_T)Length * 2 * sizeof(WCHAR)));
    if (lpText) {

        for (c = 0; c < Length; ++c) {
            x = Buffer[c];

            lpText[c * 2] = nibbletoh(x >> 4, UpcaseHex);
            lpText[c * 2 + 1] = nibbletoh(x & 15, UpcaseHex);
        }
#pragma warning(push)
#pragma warning(disable: 6305)
        lpText[Length * 2] = 0;
#pragma warning(pop)
    }

    return lpText;
}

/*
* supDestroyFileViewInfo
*
* Purpose:
*
* Deallocate file view information resources.
*
*/
VOID supDestroyFileViewInfo(
    _In_ PFILE_VIEW_INFO ViewInformation
)
{
    if (ViewInformation->FileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(ViewInformation->FileHandle);
        ViewInformation->FileHandle = INVALID_HANDLE_VALUE;
    }
    if (ViewInformation->SectionHandle) {
        NtClose(ViewInformation->SectionHandle);
        ViewInformation->SectionHandle = NULL;
    }
    if (ViewInformation->ViewBase) {
        if (NT_SUCCESS(NtUnmapViewOfSection(NtCurrentProcess(),
            ViewInformation->ViewBase)))
        {
            ViewInformation->ViewBase = NULL;
            ViewInformation->ViewSize = 0;
        }
    }

    ViewInformation->NtHeaders = NULL;
    ViewInformation->FileSize.QuadPart = 0;
}

#define PE_SIGNATURE_SIZE           4
#define RTL_MEG                     (1024UL * 1024UL)
#define RTLP_IMAGE_MAX_DOS_HEADER   (256UL * RTL_MEG)
#define MM_SIZE_OF_LARGEST_IMAGE    ((ULONG)0x77000000)
#define MM_MAXIMUM_IMAGE_HEADER     (2 * PAGE_SIZE)
#define MM_MAXIMUM_IMAGE_SECTIONS                       \
     ((MM_MAXIMUM_IMAGE_HEADER - (PAGE_SIZE + sizeof(IMAGE_NT_HEADERS))) /  \
            sizeof(IMAGE_SECTION_HEADER))

/*
* supxInitializeFileViewInfo
*
* Purpose:
*
* Open file for mapping, create section, remember file size.
*
*/
NTSTATUS supxInitializeFileViewInfo(
    _In_ PFILE_VIEW_INFO ViewInformation
)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    HANDLE fileHandle, sectionHandle = NULL;
    LARGE_INTEGER fileSize;

    fileSize.QuadPart = 0;
    fileHandle = CreateFile(ViewInformation->FileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_SUPPORTS_BLOCK_REFCOUNTING | FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (fileHandle != INVALID_HANDLE_VALUE) {

        if (!GetFileSizeEx(fileHandle, &fileSize)) {
            CloseHandle(fileHandle);
            fileHandle = INVALID_HANDLE_VALUE;
            ntStatus = STATUS_FILE_INVALID;
        }
        else {

            ntStatus = NtCreateSection(
                &sectionHandle,
                SECTION_QUERY | SECTION_MAP_READ,
                NULL,
                &fileSize,
                PAGE_READONLY,
                SEC_COMMIT,
                fileHandle);

            if (!NT_SUCCESS(ntStatus)) {
                CloseHandle(fileHandle);
                fileHandle = INVALID_HANDLE_VALUE;
            }

        }

    }
    else {
        ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
    }

    ViewInformation->Status = StatusOk;
    ViewInformation->FileHandle = fileHandle;
    ViewInformation->FileSize = fileSize;
    ViewInformation->SectionHandle = sectionHandle;

    return ntStatus;
}

/*
* supMapInputFileForRead
*
* Purpose:
*
* Create mapped section from input file.
*
*/
NTSTATUS supMapInputFileForRead(
    _In_ PFILE_VIEW_INFO ViewInformation,
    _In_ BOOLEAN PartialMap
)
{
    NTSTATUS ntStatus;
    SIZE_T viewSize;

    ntStatus = supxInitializeFileViewInfo(ViewInformation);
    if (!NT_SUCCESS(ntStatus))
        return ntStatus;

    if (PartialMap) {

        if (ViewInformation->FileSize.QuadPart < RTL_MEG)
            viewSize = (SIZE_T)ViewInformation->FileSize.QuadPart;
        else
            viewSize = (SIZE_T)RTL_MEG;

    }
    else {

        viewSize = (SIZE_T)ViewInformation->FileSize.QuadPart;

    }

    ntStatus = NtMapViewOfSection(ViewInformation->SectionHandle,
        NtCurrentProcess(),
        &ViewInformation->ViewBase,
        0,
        0,
        NULL,
        &viewSize,
        ViewShare,
        0,
        PAGE_READONLY);

    if (NT_SUCCESS(ntStatus))
        ViewInformation->ViewSize = viewSize;

    return ntStatus;
}

#pragma warning(push)
#pragma warning(disable: 4319)

/*
* supxValidateNtHeader
*
* Purpose:
*
* Common validation for file image header.
*
*/
BOOLEAN supxValidateNtHeader(
    _In_ PIMAGE_NT_HEADERS Header,
    _Out_ PIMAGE_VERIFY_STATUS VerifyStatus
)
{
    INT i;
    ULONG64 lastSectionVA;
    PIMAGE_NT_HEADERS32 pHdr32;
    PIMAGE_NT_HEADERS64 pHdr64;
    PIMAGE_SECTION_HEADER pSection;

    if (Header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {

        pHdr64 = (PIMAGE_NT_HEADERS64)(Header);

        if (((pHdr64->OptionalHeader.FileAlignment & 511) != 0) &&
            (pHdr64->OptionalHeader.FileAlignment != pHdr64->OptionalHeader.SectionAlignment))
        {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (pHdr64->OptionalHeader.FileAlignment == 0) {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (((pHdr64->OptionalHeader.SectionAlignment - 1) &
            pHdr64->OptionalHeader.SectionAlignment) != 0)
        {
            *VerifyStatus = StatusBadSectionAlignment;
            return FALSE;
        }

        if (((pHdr64->OptionalHeader.FileAlignment - 1) &
            pHdr64->OptionalHeader.FileAlignment) != 0)
        {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (pHdr64->OptionalHeader.SectionAlignment < pHdr64->OptionalHeader.FileAlignment) {
            *VerifyStatus = StatusBadSectionAlignment;
            return FALSE;
        }

        if (pHdr64->OptionalHeader.SizeOfImage > MM_SIZE_OF_LARGEST_IMAGE) {
            *VerifyStatus = StatusBadSizeOfImage;
            return FALSE;
        }

        if (pHdr64->FileHeader.NumberOfSections > MM_MAXIMUM_IMAGE_SECTIONS) {
            *VerifyStatus = StatusBadSectionCount;
            return FALSE;
        }

        if (pHdr64->FileHeader.Machine != IMAGE_FILE_MACHINE_IA64 &&
            pHdr64->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        {
            *VerifyStatus = StatusBadFileHeaderMachine;
            return FALSE;
        }

    }
    else if (Header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {

        pHdr32 = (PIMAGE_NT_HEADERS32)(Header);

        if (((pHdr32->OptionalHeader.FileAlignment & 511) != 0) &&
            (pHdr32->OptionalHeader.FileAlignment != pHdr32->OptionalHeader.SectionAlignment))
        {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (pHdr32->OptionalHeader.FileAlignment == 0) {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (((pHdr32->OptionalHeader.SectionAlignment - 1) &
            pHdr32->OptionalHeader.SectionAlignment) != 0)
        {
            *VerifyStatus = StatusBadSectionAlignment;
            return FALSE;
        }

        if (((pHdr32->OptionalHeader.FileAlignment - 1) &
            pHdr32->OptionalHeader.FileAlignment) != 0)
        {
            *VerifyStatus = StatusBadFileAlignment;
            return FALSE;
        }

        if (pHdr32->OptionalHeader.SectionAlignment < pHdr32->OptionalHeader.FileAlignment) {
            *VerifyStatus = StatusBadSectionAlignment;
            return FALSE;
        }

        if (pHdr32->OptionalHeader.SizeOfImage > MM_SIZE_OF_LARGEST_IMAGE) {
            *VerifyStatus = StatusBadSizeOfImage;
            return FALSE;
        }

        if (pHdr32->FileHeader.NumberOfSections > MM_MAXIMUM_IMAGE_SECTIONS) {
            *VerifyStatus = StatusBadSectionCount;
            return FALSE;
        }

        if ((pHdr32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) &&
            !(pHdr32->FileHeader.Machine == IMAGE_FILE_MACHINE_I386))
        {
            *VerifyStatus = StatusBadFileHeaderMachine;
            return FALSE;
        }

    }
    else {
        *VerifyStatus = StatusBadOptionalHeaderMagic;
        return FALSE;
    }

    pSection = IMAGE_FIRST_SECTION(Header);

    lastSectionVA = (ULONG64)pSection->VirtualAddress;

    for (i = 0; i < Header->FileHeader.NumberOfSections; i++, pSection++) {

        if (pSection->VirtualAddress != lastSectionVA) {
            *VerifyStatus = StatusBadNtHeaders;
            return FALSE;
        }

        lastSectionVA += ALIGN_UP_BY(pSection->Misc.VirtualSize,
            Header->OptionalHeader.SectionAlignment);

    }

    if (lastSectionVA != Header->OptionalHeader.SizeOfImage) {
        *VerifyStatus = StatusBadNtHeaders;
        return FALSE;
    }

    *VerifyStatus = StatusOk;
    return TRUE;
}

#pragma warning(pop)

/*
* supIsValidImage
*
* Purpose:
*
* Check whatever image is in valid PE format.
*
*/
BOOLEAN supIsValidImage(
    _In_ PFILE_VIEW_INFO ViewInformation
)
{
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)ViewInformation->ViewBase;
    PIMAGE_NT_HEADERS ntHeaders = NULL;

    ViewInformation->Status = StatusUnknownError;

    __try {

        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            ViewInformation->Status = StatusBadDosMagic;
            return FALSE;
        }

        if (dosHeader->e_lfanew == 0 ||
            (ULONG)dosHeader->e_lfanew > ViewInformation->FileSize.LowPart ||
            (((ULONG)dosHeader->e_lfanew + PE_SIGNATURE_SIZE +
                (ULONG)sizeof(IMAGE_FILE_HEADER)) >= ViewInformation->FileSize.LowPart) ||
            dosHeader->e_lfanew >= RTLP_IMAGE_MAX_DOS_HEADER)
        {
            ViewInformation->Status = StatusBadNewExeOffset;
            return FALSE;
        }

        if (((ULONG)dosHeader->e_lfanew +
            sizeof(IMAGE_NT_HEADERS) +
            (16 * sizeof(IMAGE_SECTION_HEADER))) <= (ULONG)dosHeader->e_lfanew)
        {
            ViewInformation->Status = StatusBadNewExeOffset;
            return FALSE;
        }

        ntHeaders = (PIMAGE_NT_HEADERS)((PCHAR)ViewInformation->ViewBase + (ULONG)dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            ViewInformation->Status = StatusBadNtSignature;
            return FALSE;
        }

        if ((ULONG)dosHeader->e_lfanew >= ntHeaders->OptionalHeader.SizeOfImage) {
            ViewInformation->Status = StatusBadNewExeOffset;
            return FALSE;
        }

        if (ntHeaders->FileHeader.SizeOfOptionalHeader == 0 ||
            ntHeaders->FileHeader.SizeOfOptionalHeader & (sizeof(ULONG_PTR) - 1))
        {
            ViewInformation->Status = StatusBadOptionalHeader;
            return FALSE;
        }

        if (!(ntHeaders->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)) {
            ViewInformation->Status = StatusBadFileHeaderCharacteristics;
            return FALSE;
        }

        if (ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 &&
            ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        {
            ViewInformation->Status = StatusBadFileHeaderMachine;
            return FALSE;
        }

        return supxValidateNtHeader(ntHeaders, &ViewInformation->Status);

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ViewInformation->Status = StatusExceptionOccurred;
        return FALSE;
    }
}

/*
* supxCreateDriverEntry
*
* Purpose:
*
* Creating registry entry for driver.
*
*/
NTSTATUS supxCreateDriverEntry(
    _In_opt_ LPCWSTR DriverPath,
    _In_ LPCWSTR KeyName
)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    DWORD dwData, dwResult;
    HKEY keyHandle = NULL;
    UNICODE_STRING driverImagePath;

    RtlInitEmptyUnicodeString(&driverImagePath, NULL, 0);

    if (DriverPath) {
        if (!RtlDosPathNameToNtPathName_U(DriverPath,
            &driverImagePath,
            NULL,
            NULL))
        {
            return STATUS_INVALID_PARAMETER_2;
        }
    }

    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        KeyName,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &keyHandle,
        NULL))
    {
        status = STATUS_ACCESS_DENIED;
        goto Cleanup;
    }

    dwResult = ERROR_SUCCESS;

    do {

        dwData = SERVICE_ERROR_NORMAL;
        dwResult = RegSetValueEx(keyHandle,
            TEXT("ErrorControl"),
            0,
            REG_DWORD,
            (BYTE*)&dwData,
            sizeof(dwData));
        if (dwResult != ERROR_SUCCESS)
            break;

        dwData = SERVICE_KERNEL_DRIVER;
        dwResult = RegSetValueEx(keyHandle,
            TEXT("Type"),
            0,
            REG_DWORD,
            (BYTE*)&dwData,
            sizeof(dwData));
        if (dwResult != ERROR_SUCCESS)
            break;

        dwData = SERVICE_DEMAND_START;
        dwResult = RegSetValueEx(keyHandle,
            TEXT("Start"),
            0,
            REG_DWORD,
            (BYTE*)&dwData,
            sizeof(dwData));

        if (dwResult != ERROR_SUCCESS)
            break;

        if (DriverPath) {
            dwResult = RegSetValueEx(keyHandle,
                TEXT("ImagePath"),
                0,
                REG_EXPAND_SZ,
                (BYTE*)driverImagePath.Buffer,
                (DWORD)driverImagePath.Length + sizeof(UNICODE_NULL));
        }

    } while (FALSE);

    RegCloseKey(keyHandle);

    if (dwResult != ERROR_SUCCESS) {
        status = STATUS_ACCESS_DENIED;
    }
    else
    {
        status = STATUS_SUCCESS;
    }

Cleanup:
    if (DriverPath) {
        if (driverImagePath.Buffer) {
            RtlFreeUnicodeString(&driverImagePath);
        }
    }
    return status;
}

/*
* supLoadDriverEx
*
* Purpose:
*
* Install driver and load it.
*
* N.B.
* SE_LOAD_DRIVER_PRIVILEGE is required to be assigned and enabled.
*
*/
NTSTATUS supLoadDriverEx(
    _In_ LPCWSTR DriverName,
    _In_ LPCWSTR DriverPath,
    _In_ BOOLEAN UnloadPreviousInstance,
    _In_opt_ pfnLoadDriverCallback Callback,
    _In_opt_ PVOID CallbackParam
)
{
    SIZE_T keyOffset;
    NTSTATUS status;
    UNICODE_STRING driverServiceName;

    WCHAR szBuffer[MAX_PATH + 1];

    if (DriverName == NULL)
        return STATUS_INVALID_PARAMETER_1;
    if (DriverPath == NULL)
        return STATUS_INVALID_PARAMETER_2;

    RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));

    keyOffset = RTL_NUMBER_OF(NT_REG_PREP);

    if (FAILED(RtlStringCchPrintfSecure(szBuffer, MAX_PATH,
        DRIVER_REGKEY,
        NT_REG_PREP,
        DriverName)))
    {
        return STATUS_INVALID_PARAMETER_1;
    }

    status = supxCreateDriverEntry(DriverPath,
        &szBuffer[keyOffset]);

    if (!NT_SUCCESS(status))
        return status;

    RtlInitUnicodeString(&driverServiceName, szBuffer);

    if (Callback) {
        status = Callback(&driverServiceName, CallbackParam);
        if (!NT_SUCCESS(status))
            return status;
    }

    if (supEnablePrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE)) {

        status = NtLoadDriver(&driverServiceName);

        if (UnloadPreviousInstance) {
            if ((status == STATUS_IMAGE_ALREADY_LOADED) ||
                (status == STATUS_OBJECT_NAME_COLLISION) ||
                (status == STATUS_OBJECT_NAME_EXISTS))
            {
                status = NtUnloadDriver(&driverServiceName);
                if (NT_SUCCESS(status)) {
                    status = NtLoadDriver(&driverServiceName);
                }
            }
        }
        else {
            if (status == STATUS_OBJECT_NAME_EXISTS)
                status = STATUS_SUCCESS;
        }

        supEnablePrivilege(SE_LOAD_DRIVER_PRIVILEGE, FALSE);
    }
    else {
        status = STATUS_PRIVILEGE_NOT_HELD;
    }

    return status;
}

/*
* supLoadDriver
*
* Purpose:
*
* Install driver and load it.
*
* N.B.
* SE_LOAD_DRIVER_PRIVILEGE is required to be assigned and enabled.
*
*/
NTSTATUS supLoadDriver(
    _In_ LPCWSTR DriverName,
    _In_ LPCWSTR DriverPath,
    _In_ BOOLEAN UnloadPreviousInstance
)
{
    return supLoadDriverEx(DriverName,
        DriverPath,
        UnloadPreviousInstance,
        NULL,
        NULL);
}

/*
* supUnloadDriver
*
* Purpose:
*
* Call driver unload and remove corresponding registry key.
*
* N.B.
* SE_LOAD_DRIVER_PRIVILEGE is required to be assigned and enabled.
*
*/
NTSTATUS supUnloadDriver(
    _In_ LPCWSTR DriverName,
    _In_ BOOLEAN fRemove
)
{
    NTSTATUS status;
    SIZE_T keyOffset;
    UNICODE_STRING driverServiceName;

    WCHAR szBuffer[MAX_PATH + 1];

    RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));

    if (FAILED(RtlStringCchPrintfSecure(szBuffer, MAX_PATH,
        DRIVER_REGKEY,
        NT_REG_PREP,
        DriverName)))
    {
        return STATUS_INVALID_PARAMETER_1;
    }

    keyOffset = RTL_NUMBER_OF(NT_REG_PREP);

    status = supxCreateDriverEntry(NULL,
        &szBuffer[keyOffset]);

    if (!NT_SUCCESS(status))
        return status;

    if (supEnablePrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE)) {

        RtlInitUnicodeString(&driverServiceName, szBuffer);
        status = NtUnloadDriver(&driverServiceName);

        supEnablePrivilege(SE_LOAD_DRIVER_PRIVILEGE, FALSE);
    }
    else {
        status = STATUS_PRIVILEGE_NOT_HELD;
    }

    if (NT_SUCCESS(status)) {
        if (fRemove)
            supRegDeleteKeyRecursive(HKEY_LOCAL_MACHINE, &szBuffer[keyOffset]);
    }

    return status;
}

/*
* supOpenDriverEx
*
* Purpose:
*
* Open handle for driver.
*
*/
NTSTATUS supOpenDriverEx(
    _In_ LPCWSTR DriverName,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_opt_ PHANDLE DeviceHandle
)
{
    HANDLE deviceHandle = NULL;
    UNICODE_STRING usDeviceLink;
    OBJECT_ATTRIBUTES obja;
    IO_STATUS_BLOCK iost;

    NTSTATUS ntStatus;

    RtlInitUnicodeString(&usDeviceLink, DriverName);
    InitializeObjectAttributes(&obja, &usDeviceLink, OBJ_CASE_INSENSITIVE, NULL, NULL);

    ntStatus = NtCreateFile(&deviceHandle,
        DesiredAccess,
        &obja,
        &iost,
        NULL,
        0,
        0,
        FILE_OPEN,
        0,
        NULL,
        0);

    if (NT_SUCCESS(ntStatus)) {
        if (DeviceHandle)
            *DeviceHandle = deviceHandle;
    }

    return ntStatus;
}

/*
* supOpenDriver
*
* Purpose:
*
* Open handle for driver through \\DosDevices.
*
*/
NTSTATUS supOpenDriver(
    _In_ LPCWSTR DriverName,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE DeviceHandle
)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    WCHAR szDeviceLink[MAX_PATH + 1];

    // assume failure
    if (DeviceHandle)
        *DeviceHandle = NULL;
    else
        return STATUS_INVALID_PARAMETER_2;

    if (DriverName) {

        RtlSecureZeroMemory(szDeviceLink, sizeof(szDeviceLink));

        if (FAILED(RtlStringCchPrintfSecure(szDeviceLink,
            MAX_PATH,
            TEXT("\\DosDevices\\%wS"),
            DriverName)))
        {
            return STATUS_INVALID_PARAMETER_1;
        }

        status = supOpenDriverEx(szDeviceLink,
            DesiredAccess,
            DeviceHandle);

        if (status == STATUS_OBJECT_NAME_NOT_FOUND ||
            status == STATUS_NO_SUCH_DEVICE)
        {

            //
            // Check the case when no symlink available.
            //

            RtlSecureZeroMemory(szDeviceLink, sizeof(szDeviceLink));

            if (FAILED(RtlStringCchPrintfSecure(szDeviceLink,
                MAX_PATH,
                TEXT("\\Device\\%wS"),
                DriverName)))
            {
                return STATUS_INVALID_PARAMETER_1;
            }

            status = supOpenDriverEx(szDeviceLink,
                DesiredAccess,
                DeviceHandle);

        }

    }
    else {
        status = STATUS_INVALID_PARAMETER_1;
    }

    return status;
}

/*
* supDeleteFileWithWait
*
* Purpose:
*
* Removes file from disk.
*
*/
BOOL supDeleteFileWithWait(
    _In_ ULONG WaitMilliseconds,
    _In_ ULONG NumberOfAttempts,
    _In_ LPCWSTR lpFileName
)
{
    ULONG retryCount = NumberOfAttempts;

    do {

        Sleep(WaitMilliseconds);
        if (DeleteFile(lpFileName)) {
            return TRUE;
        }

        retryCount--;

    } while (retryCount);

    return FALSE;
}

/*
* supCallDriver
*
* Purpose:
*
* Call driver.
*
*/
NTSTATUS supCallDriver(
    _In_ HANDLE DeviceHandle,
    _In_ ULONG IoControlCode,
    _In_opt_ PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _In_opt_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength)
{
    IO_STATUS_BLOCK ioStatus;

    return NtDeviceIoControlFile(DeviceHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        IoControlCode,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength);
}

/*
* supIsLongTermServicingWindows
*
* Purpose:
*
* Attempt to detect LTSC/LTSB product type.
*
*/
BOOLEAN supIsLongTermServicingWindows(
    VOID
)
{
    NTSTATUS ntStatus;
    ULONG dataLicense = 0, dataSize = 0, dataType = 0, i;

    UNICODE_STRING usLicenseValue = RTL_CONSTANT_STRING(L"Kernel-ProductInfo");

    DWORD suiteType[] = {
        PRODUCT_ENTERPRISE_S,              // LTSB/C
        PRODUCT_ENTERPRISE_S_N,            // LTSB/C N
        PRODUCT_ENTERPRISE_S_EVALUATION,   // LTSB/C Evaluation
        PRODUCT_ENTERPRISE_S_N_EVALUATION, // LTSB/C N Evaluation
        PRODUCT_IOTENTERPRISES             // IoT Enterprise LTSC
    };

    ntStatus = NtQueryLicenseValue(
        &usLicenseValue,
        &dataType,
        (PVOID)&dataLicense,
        sizeof(DWORD),
        &dataSize);

    if (NT_SUCCESS(ntStatus) &&
        dataType == REG_DWORD &&
        dataSize == sizeof(DWORD))
    {
        for (i = 0; i < RTL_NUMBER_OF(suiteType); i++) {
            if (dataLicense == suiteType[i]) {
                return TRUE;
            }
        }
    }

    return FALSE;
}
