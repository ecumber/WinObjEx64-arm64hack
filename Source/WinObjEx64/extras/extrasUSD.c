/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2015 - 2021
*
*  TITLE:       EXTRASUSD.C
*
*  VERSION:     1.92
*
*  DATE:        18 Sep 2021
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"
#include "propObjectDump.h"
#include "extras.h"
#include "extrasUSD.h"
#include "treelist/treelist.h"

EXTRASCONTEXT g_UsdDlgContext;

/*
* UsdDumpSharedRegion
*
* Purpose:
*
* Display dump of SharedData.
*
*/
VOID UsdDumpSharedRegion(
    _In_ HWND hwndParent
)
{
    BOOL                bAny = FALSE;
    UINT                i;
    DWORD               mask, cFlags;

    LPCWSTR             *pvSharedFlagsDesc;

    HTREEITEM           h_tviRootItem, h_tviSubItem, h_tviLast = NULL;
    LPWSTR              lpType;
    TL_SUBITEMS_FIXED   subitems;
    TVITEMEX            itemex;
    WCHAR               szValue[MAX_PATH + 1];

    PKUSER_SHARED_DATA  pUserSharedData;


    do {

        pUserSharedData = (KUSER_SHARED_DATA* const)MM_SHARED_USER_DATA_VA;

        if (IsBadReadPtr(pUserSharedData, sizeof(KUSER_SHARED_DATA)))
            break;

        if (!supInitTreeListForDump(hwndParent, &g_UsdDlgContext.TreeList))
            break;

        //
        // KUSER_SHARED_DATA
        //

        h_tviRootItem = supTreeListAddItem(
            g_UsdDlgContext.TreeList,
            (HTREEITEM)NULL,
            TVIF_TEXT | TVIF_STATE,
            TVIS_EXPANDED,
            TVIS_EXPANDED,
            TEXT("KUSER_SHARED_DATA"),
            (PVOID)NULL);

        if (h_tviRootItem == NULL) {
            break;
        }

        //
        // NtSystemRoot
        //
        RtlSecureZeroMemory(&subitems, sizeof(subitems));
        subitems.Text[0] = pUserSharedData->NtSystemRoot;
        subitems.Count = 1;

        supTreeListAddItem(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TVIF_TEXT | TVIF_STATE,
            (UINT)0,
            (UINT)0,
            TEXT("NtSystemRoot"),
            &subitems);

        //
        // NtProductType
        //
        switch (pUserSharedData->NtProductType) {
        case NtProductWinNt:
            lpType = TEXT("NtProductWinNt");
            break;
        case NtProductLanManNt:
            lpType = TEXT("NtProductLanManNt");
            break;
        case NtProductServer:
            lpType = TEXT("NtProductServer");
            break;
        default:
            lpType = T_UnknownType;
            break;
        }

        propObDumpUlong(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("NtProductType"),
            lpType,
            pUserSharedData->NtProductType,
            FALSE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        propObDumpByte(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("ProductTypeIsValid"),
            (LPWSTR)NULL,
            pUserSharedData->ProductTypeIsValid,
            (COLORREF)0,
            (COLORREF)0,
            TRUE);

        //
        // NtMajorVersion
        //
        propObDumpUlong(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("NtMajorVersion"),
            (LPWSTR)NULL,
            pUserSharedData->NtMajorVersion,
            FALSE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        //
        // NtMinorVersion
        // 
        propObDumpUlong(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("NtMinorVersion"),
            (LPWSTR)NULL,
            pUserSharedData->NtMinorVersion,
            FALSE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        //
        // Prior to Windows 10 this field declared as reserved.
        //
        if (g_NtBuildNumber >= NT_WIN10_THRESHOLD1) {
            propObDumpUlong(
                g_UsdDlgContext.TreeList,
                h_tviRootItem,
                TEXT("NtBuildNumber"),
                (LPWSTR)NULL,
                pUserSharedData->NtBuildNumber,
                FALSE,
                FALSE,
                (COLORREF)0,
                (COLORREF)0);
        }

        //
        // ProcessorFeatures
        //
        h_tviSubItem = supTreeListAddItem(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TVIF_TEXT | TVIF_STATE,
            (UINT)0,
            (UINT)0,
            TEXT("ProcessorFeatures"),
            (PVOID)NULL);

        if (h_tviSubItem) {
            for (i = 0; i < PROCESSOR_FEATURE_MAX; i++) {
                if (pUserSharedData->ProcessorFeatures[i]) {
                    bAny = TRUE;
                    if (i >= RTL_NUMBER_OF(T_PROCESSOR_FEATURES)) {
                        lpType = T_Unknown;
                    }
                    else {
                        lpType = T_PROCESSOR_FEATURES[i];
                    }
                    RtlSecureZeroMemory(&subitems, sizeof(subitems));
                    RtlSecureZeroMemory(&szValue, sizeof(szValue));
                    ultostr(i, szValue);
                    subitems.Text[0] = szValue;
                    subitems.Text[1] = lpType;
                    subitems.Count = 2;
                    h_tviLast = supTreeListAddItem(
                        g_UsdDlgContext.TreeList,
                        h_tviSubItem,
                        TVIF_TEXT | TVIF_STATE,
                        (UINT)0,
                        (UINT)0,
                        (LPWSTR)T_EmptyString,
                        &subitems);
                }
            }

            //
            // Output dotted corner for processor features.
            //
            if (h_tviLast) {
                RtlSecureZeroMemory(&itemex, sizeof(itemex));

                itemex.hItem = h_tviLast;
                itemex.mask = TVIF_TEXT | TVIF_HANDLE;
                itemex.pszText = T_EMPTY;

                TreeList_SetTreeItem(g_UsdDlgContext.TreeList, &itemex, NULL);
            }

            if (bAny == FALSE) {
                RtlSecureZeroMemory(&subitems, sizeof(subitems));
                lpType = TEXT("-");
                _strcpy(szValue, TEXT("0"));
                subitems.Text[0] = szValue;
                subitems.Text[1] = lpType;
                subitems.Count = 2;
                supTreeListAddItem(
                    g_UsdDlgContext.TreeList,
                    h_tviSubItem,
                    TVIF_TEXT | TVIF_STATE,
                    (UINT)0,
                    (UINT)0,
                    (LPWSTR)T_EmptyString,
                    &subitems);
            }
        }

        //
        // AlternativeArchitecture
        //
        switch (pUserSharedData->AlternativeArchitecture) {
        case StandardDesign:
            lpType = TEXT("StandardDesign");
            break;
        case NEC98x86:
            lpType = TEXT("NEC98x86");
            break;
        default:
            lpType = T_UnknownType;
            break;
        }

        propObDumpUlong(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("AlternativeArchitecture"),
            lpType,
            pUserSharedData->AlternativeArchitecture,
            FALSE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        //
        // SuiteMask
        //
        RtlSecureZeroMemory(&subitems, sizeof(subitems));
        RtlSecureZeroMemory(&szValue, sizeof(szValue));
        szValue[0] = TEXT('0');
        szValue[1] = TEXT('x');
        ultohex(pUserSharedData->SuiteMask, &szValue[2]);
        subitems.Text[0] = szValue;
        subitems.Count = 1;

        h_tviSubItem = supTreeListAddItem(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TVIF_TEXT | TVIF_STATE,
            (UINT)0,
            (UINT)0,
            TEXT("SuiteMask"),
            &subitems);

        if (h_tviSubItem) {
            h_tviLast = NULL;
            mask = pUserSharedData->SuiteMask;
            for (i = 0; i < MAX_KNOWN_SUITEMASKS; i++) {
                if (mask & SuiteMasks[i].dwValue) {

                    RtlSecureZeroMemory(&subitems, sizeof(subitems));
                    RtlSecureZeroMemory(&szValue, sizeof(szValue));
                    szValue[0] = TEXT('0');
                    szValue[1] = TEXT('x');
                    ultohex(SuiteMasks[i].dwValue, &szValue[2]);
                    subitems.Text[0] = szValue;
                    subitems.Text[1] = SuiteMasks[i].lpDescription;
                    subitems.Count = 2;

                    h_tviLast = supTreeListAddItem(
                        g_UsdDlgContext.TreeList,
                        h_tviSubItem,
                        TVIF_TEXT | TVIF_STATE,
                        (UINT)0,
                        (UINT)0,
                        (LPWSTR)T_EmptyString,
                        &subitems);

                    mask &= ~SuiteMasks[i].dwValue;
                }
            }

            //
            // Output dotted corner for suite mask.
            //
            if (h_tviLast) {
                RtlSecureZeroMemory(&itemex, sizeof(itemex));

                itemex.hItem = h_tviLast;
                itemex.mask = TVIF_TEXT | TVIF_HANDLE;
                itemex.pszText = T_EMPTY;

                TreeList_SetTreeItem(g_UsdDlgContext.TreeList, &itemex, NULL);
            }
        }

        //
        // KdDebuggerEnabled
        //
        propObDumpByte(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("KdDebuggerEnabled"),
            (LPWSTR)NULL,
            pUserSharedData->KdDebuggerEnabled,
            (COLORREF)0,
            (COLORREF)0,
            TRUE);

        //
        // MitigationPolicies
        //

        if (g_NtBuildNumber < NT_WIN8_RTM) {

            propObDumpByte(
                g_UsdDlgContext.TreeList,
                h_tviRootItem,
                TEXT("NXSupportPolicy"),
                (LPWSTR)NULL,
                pUserSharedData->NXSupportPolicy,
                (COLORREF)0,
                (COLORREF)0,
                FALSE);

        }
        else {

            //
            // Expanded to more values starting from Windows 8+
            //

            RtlSecureZeroMemory(&subitems, sizeof(subitems));
            RtlSecureZeroMemory(szValue, sizeof(szValue));

            RtlStringCchPrintfSecure(szValue,
                MAX_PATH,
                TEXT("0x%02X"),
                pUserSharedData->MitigationPolicies);

            subitems.Text[0] = szValue;
            subitems.Count = 1;

            h_tviSubItem = supTreeListAddItem(
                g_UsdDlgContext.TreeList,
                h_tviRootItem,
                TVIF_TEXT | TVIF_STATE,
                (UINT)0,
                (UINT)0,
                TEXT("MitigationPolicies"),
                &subitems);

            if (h_tviSubItem) {

                propObDumpByte(
                    g_UsdDlgContext.TreeList,
                    h_tviSubItem,
                    TEXT("NXSupportPolicy"),
                    (LPWSTR)NULL,
                    pUserSharedData->NXSupportPolicy,
                    (COLORREF)0,
                    (COLORREF)0,
                    FALSE);

                propObDumpByte(
                    g_UsdDlgContext.TreeList,
                    h_tviSubItem,
                    TEXT("SEHValidationPolicy"),
                    (LPWSTR)NULL,
                    pUserSharedData->SEHValidationPolicy,
                    (COLORREF)0,
                    (COLORREF)0,
                    FALSE);


                propObDumpByte(
                    g_UsdDlgContext.TreeList,
                    h_tviSubItem,
                    TEXT("CurDirDevicesSkippedForDlls"),
                    (LPWSTR)NULL,
                    pUserSharedData->CurDirDevicesSkippedForDlls,
                    (COLORREF)0,
                    (COLORREF)0,
                    FALSE);
            }
        }

        //
        // ActiveConsoleId
        //
        propObDumpUlong(g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("ActiveConsoleId"),
            NULL,
            pUserSharedData->ActiveConsoleId,
            TRUE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        //
        // SafeBootMode
        //
        propObDumpByte(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("SafeBootMode"),
            (LPWSTR)NULL,
            pUserSharedData->SafeBootMode,
            (COLORREF)0,
            (COLORREF)0,
            TRUE);

        //
        // SharedDataFlags
        //
        RtlSecureZeroMemory(&subitems, sizeof(subitems));
        RtlSecureZeroMemory(&szValue, sizeof(szValue));
        szValue[0] = TEXT('0');
        szValue[1] = TEXT('x');
        ultohex(pUserSharedData->SharedDataFlags, &szValue[2]);
        subitems.Text[0] = szValue;
        subitems.Count = 1;

        h_tviSubItem = supTreeListAddItem(
            g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TVIF_TEXT | TVIF_STATE,
            (UINT)0,
            (UINT)0,
            TEXT("SharedDataFlags"),
            &subitems);

        if (h_tviSubItem) {

            h_tviLast = NULL;

            if (g_NtBuildNumber < NT_WIN8_RTM) {
                pvSharedFlagsDesc = T_SharedDataFlagsW7;
                cFlags = RTL_NUMBER_OF(T_SharedDataFlagsW7);
            }
            else {
                pvSharedFlagsDesc = T_SharedDataFlags;
                cFlags = RTL_NUMBER_OF(T_SharedDataFlags);
            }

            for (i = 0; i < cFlags; i++) {
                if (GET_BIT(pUserSharedData->SharedDataFlags, i)) {
                    RtlSecureZeroMemory(&subitems, sizeof(subitems));
                    RtlSecureZeroMemory(&szValue, sizeof(szValue));
                    _strcpy(szValue, TEXT("BitPos: "));
                    ultostr(i, _strend(szValue));
                    subitems.Text[0] = szValue;
                    subitems.Text[1] = (LPWSTR)pvSharedFlagsDesc[i];
                    subitems.Count = 2;
                    h_tviLast = supTreeListAddItem(
                        g_UsdDlgContext.TreeList,
                        h_tviSubItem,
                        TVIF_TEXT | TVIF_STATE,
                        (UINT)0,
                        (UINT)0,
                        (LPWSTR)T_EmptyString,
                        &subitems);
                }
            }

            //
            // Output dotted corner for shared data flags
            //
            if (h_tviLast) {
                RtlSecureZeroMemory(&itemex, sizeof(itemex));

                itemex.hItem = h_tviLast;
                itemex.mask = TVIF_TEXT | TVIF_HANDLE;
                itemex.pszText = T_EMPTY;

                TreeList_SetTreeItem(g_UsdDlgContext.TreeList, &itemex, NULL);
            }

        }

        //
        // Cookie
        //
        propObDumpUlong(g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("Cookie"),
            NULL,
            pUserSharedData->Cookie,
            TRUE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

        //
        // ActiveProcessorCount
        //
        propObDumpUlong(g_UsdDlgContext.TreeList,
            h_tviRootItem,
            TEXT("ActiveProcessorCount"),
            NULL,
            pUserSharedData->ActiveProcessorCount,
            FALSE,
            FALSE,
            (COLORREF)0,
            (COLORREF)0);

    } while (FALSE);
}

/*
* UsdDialogHandlePopupMenu
*
* Purpose:
*
* Treelist popup construction
*
*/
VOID UsdDialogHandlePopupMenu(
    _In_ HWND hwndDlg,
    _In_ LPARAM lParam
)
{
    HMENU hMenu;
    POINT pt1;

    if (GetCursorPos(&pt1) == FALSE)
        return;

    hMenu = CreatePopupMenu();
    if (hMenu) {

        if (supTreeListAddCopyValueItem(hMenu,
            g_UsdDlgContext.TreeList,
            ID_OBJECT_COPY,
            0,
            lParam,
            &g_UsdDlgContext.tlSubItemHit))
        {
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt1.x, pt1.y, 0, hwndDlg, NULL);
        }

        DestroyMenu(hMenu);

    }
}

/*
* UsdDialogProc
*
* Purpose:
*
* Usd Dialog Procedure
*
*/
INT_PTR CALLBACK UsdDialogProc(
    _In_  HWND hwndDlg,
    _In_  UINT uMsg,
    _In_  WPARAM wParam,
    _In_  LPARAM lParam
)
{
    switch (uMsg) {

    case WM_INITDIALOG:
        supCenterWindow(hwndDlg);
        break;

    case WM_CLOSE:
        DestroyWindow(g_UsdDlgContext.TreeList);
        DestroyWindow(hwndDlg);
        g_WinObj.AuxDialogs[wobjUSDDlgId] = NULL;
        break;

    case WM_COMMAND:

        switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case IDCANCEL:

            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            return TRUE;

        case ID_OBJECT_COPY:

            supTreeListCopyItemValueToClipboard(g_UsdDlgContext.TreeList,
                g_UsdDlgContext.tlSubItemHit);

            break;

        default:
            break;
        }

        break;

    case WM_CONTEXTMENU:

        UsdDialogHandlePopupMenu(hwndDlg, lParam);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/*
* extrasCreateUsdDialog
*
* Purpose:
*
* Create and initialize Usd Dialog.
*
*/
VOID extrasCreateUsdDialog(
    _In_ HWND hwndParent
)
{
    //
    // Allow only one dialog.
    //
    ENSURE_DIALOG_UNIQUE(g_WinObj.AuxDialogs[wobjUSDDlgId]);

    RtlSecureZeroMemory(&g_UsdDlgContext, sizeof(g_UsdDlgContext));
    g_UsdDlgContext.hwndDlg = CreateDialogParam(g_WinObj.hInstance, MAKEINTRESOURCE(IDD_DIALOG_USD),
        hwndParent, &UsdDialogProc, 0);

    if (g_UsdDlgContext.hwndDlg == NULL)
        return;

    g_WinObj.AuxDialogs[wobjUSDDlgId] = g_UsdDlgContext.hwndDlg;

    g_UsdDlgContext.tlSubItemHit = -1;

    UsdDumpSharedRegion(g_UsdDlgContext.hwndDlg);
}
