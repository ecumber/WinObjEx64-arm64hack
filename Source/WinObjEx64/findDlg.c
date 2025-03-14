/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2015 - 2021
*
*  TITLE:       FINDDLG.C
*
*  VERSION:     1.92
*
*  DATE:        07 Dec 2021
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"
#include "findDlg.h"

#define FINDDLG_TRACKSIZE_MIN_X 548
#define FINDDLG_TRACKSIZE_MIN_Y 230

typedef struct _FINDDLG_CONTEXT {
    //
    // Dialog controls and resources.
    //
    HWND DialogWindow;
    HWND StatusBar;
    HWND SearchList;
    HICON DialogIcon;

    INT ColumnCount;

    //
    // ListView selection.
    //
    INT iSelectedItem;
    INT iColumnHit;

    // ListView settings.
    INT SortColumn;
    BOOL SortInverse;

    //
    // Resize.
    //
    LONG sizes_init;
    LONG dx1;
    LONG dx2;
    LONG dx3;
    LONG dx4;
    LONG dx5;
    LONG dx6;
    LONG dx7;
    LONG dx8;
    LONG dx9;
    LONG dx10;
    LONG dx11;
    LONG dx12;
    LONG dx13;
} FINDDLG_CONTEXT, * PFINDDLGCONTEXT;

static FINDDLG_CONTEXT g_FindDlgContext;

/*
* FindDlgCompareFunc
*
* Purpose:
*
* FindDlg listview comparer function.
*
*/
INT CALLBACK FindDlgCompareFunc(
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort
)
{
    return supListViewBaseComparer(g_FindDlgContext.SearchList,
        g_FindDlgContext.SortInverse,
        lParam1,
        lParam2,
        lParamSort);
}

/*
* FindDlgAddListItem
*
* Purpose:
*
* Add item to listview.
*
*/
VOID FindDlgAddListItem(
    _In_ HWND	hList,
    _In_ LPWSTR	ObjectName,
    _In_ LPWSTR	TypeName
)
{
    INT     lvItemIndex;
    LVITEM  lvItem;

    RtlSecureZeroMemory(&lvItem, sizeof(lvItem));

    lvItem.mask = LVIF_TEXT | LVIF_IMAGE;
    lvItem.pszText = ObjectName;
    lvItem.iImage = ObManagerGetImageIndexByTypeName(TypeName);
    lvItem.iItem = MAXINT;
    lvItemIndex = ListView_InsertItem(hList, &lvItem);

    lvItem.mask = LVIF_TEXT;
    lvItem.iSubItem = 1;
    lvItem.pszText = TypeName;
    lvItem.iItem = lvItemIndex;
    ListView_SetItem(hList, &lvItem);
}

/*
* FindDlgResize
*
* Purpose:
*
* FindDlg WM_SIZE handler, remember control position and move them according new window coordinates.
*
*/
VOID FindDlgResize(
    _In_ HWND hwndDlg,
    _In_ FINDDLG_CONTEXT* Context
)
{
    RECT  r1, r2;
    HWND  hwnd;
    POINT p0;

    GetClientRect(hwndDlg, &r2);

    if (Context->sizes_init == 0) {
        Context->sizes_init = 1;

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_GROUPBOXOPTIONS), &r1);
        Context->dx1 = r2.right - (r1.right - r1.left);
        Context->dx2 = r1.bottom - r1.top;

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_GROUPBOX), &r1);
        Context->dx3 = r2.bottom - (r1.bottom - r1.top);

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_LIST), &r1);
        Context->dx4 = r2.right - (r1.right - r1.left);
        Context->dx5 = r2.bottom - (r1.bottom - r1.top);

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_NAME), &r1);
        Context->dx6 = r2.right - (r1.right - r1.left);
        Context->dx7 = r1.bottom - r1.top;

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_TYPE), &r1);
        p0.x = r1.left;
        p0.y = r1.top;
        ScreenToClient(hwndDlg, &p0);
        Context->dx8 = r2.right - p0.x;
        Context->dx9 = p0.y;

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_FIND), &r1);
        p0.x = r1.left;
        p0.y = r1.top;
        ScreenToClient(hwndDlg, &p0);
        Context->dx10 = r2.right - p0.x;
        Context->dx11 = p0.y;

        RtlSecureZeroMemory(&r1, sizeof(r1));
        GetWindowRect(GetDlgItem(hwndDlg, ID_SEARCH_TYPELABEL), &r1);
        p0.x = r1.left;
        p0.y = r1.top;
        ScreenToClient(hwndDlg, &p0);
        Context->dx12 = r2.right - p0.x;
        Context->dx13 = p0.y;
    }

    //resize groupbox search options
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_GROUPBOXOPTIONS);
    if (hwnd) {
        SetWindowPos(hwnd, 0, 0, 0,
            r2.right - Context->dx1, Context->dx2,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    //resize groupbox results
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_GROUPBOX);
    if (hwnd) {
        SetWindowPos(hwnd, 0, 0, 0,
            r2.right - Context->dx1, r2.bottom - Context->dx3,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    //resize listview
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_LIST);
    if (hwnd) {
        SetWindowPos(hwnd, 0, 0, 0,
            r2.right - Context->dx4, r2.bottom - Context->dx5,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    //resize edit
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_NAME);
    if (hwnd) {
        SetWindowPos(hwnd, 0, 0, 0,
            r2.right - Context->dx6, Context->dx7,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    //resize combobox
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_TYPE);
    if (hwnd) {
        SetWindowPos(hwnd, 0,
            r2.right - Context->dx8, Context->dx9,
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER);
    }

    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_FIND);
    if (hwnd) {
        SetWindowPos(hwnd, 0,
            r2.right - Context->dx10, Context->dx11,
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER);
    }

    //resize Type label
    hwnd = GetDlgItem(hwndDlg, ID_SEARCH_TYPELABEL);
    if (hwnd) {
        SetWindowPos(hwnd, 0,
            r2.right - Context->dx12, Context->dx13,
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER);
    }

    SendMessage(Context->StatusBar, WM_SIZE, 0, 0);

    RedrawWindow(hwndDlg, NULL, 0, RDW_ERASE | RDW_INVALIDATE | RDW_ERASENOW);
}

/*
* FindDlgHandleNotify
*
* Purpose:
*
* WM_NOTIFY processing for FindDlg listview.
*
*/
BOOL FindDlgHandleNotify(
    _In_ LPNMLISTVIEW pListView
)
{
    INT      nImageIndex;
    LPWSTR   lpItemText;

    if (pListView->hdr.idFrom != ID_SEARCH_LIST)
        return FALSE;

    switch (pListView->hdr.code) {

    case LVN_ITEMCHANGED:

        if ((pListView->uNewState & LVIS_SELECTED) &&
            !(pListView->uOldState & LVIS_SELECTED))
        {

            lpItemText = supGetItemText(pListView->hdr.hwndFrom,
                pListView->iItem,
                0,
                NULL);

            if (lpItemText) {
                ListToObject(lpItemText);
                supHeapFree(lpItemText);
            }
        }

        break;

    case LVN_COLUMNCLICK:
        g_FindDlgContext.SortInverse = !g_FindDlgContext.SortInverse;
        g_FindDlgContext.SortColumn = pListView->iSubItem;
        ListView_SortItemsEx(g_FindDlgContext.SearchList, &FindDlgCompareFunc, g_FindDlgContext.SortColumn);

        nImageIndex = ImageList_GetImageCount(g_ListViewImages);
        if (g_FindDlgContext.SortInverse)
            nImageIndex -= 2;
        else
            nImageIndex -= 1;

        supUpdateLvColumnHeaderImage(
            g_FindDlgContext.SearchList,
            g_FindDlgContext.ColumnCount,
            g_FindDlgContext.SortColumn,
            nImageIndex);

        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/*
* FindDlgHandleSettingsChange
*
* Purpose:
*
* Handle global settings change.
*
*/
VOID FindDlgHandleSettingsChange(
    _In_ FINDDLG_CONTEXT* Context
)
{
    DWORD lvExStyle;

    lvExStyle = ListView_GetExtendedListViewStyle(Context->SearchList);
    if (g_WinObj.ListViewDisplayGrid)
        lvExStyle |= LVS_EX_GRIDLINES;
    else
        lvExStyle &= ~LVS_EX_GRIDLINES;

    ListView_SetExtendedListViewStyle(Context->SearchList, lvExStyle);
}

/*
* FindDlgHandlePopupMenu
*
* Purpose:
*
* Search list popup construction.
*
*/
VOID FindDlgHandlePopupMenu(
    _In_ HWND hwndDlg,
    _In_ LPPOINT lpPoint,
    _In_ PVOID lpUserParam
)
{
    HMENU hMenu;
    UINT uPos = 0;
    FINDDLG_CONTEXT* Context = (FINDDLG_CONTEXT*)lpUserParam;

    hMenu = CreatePopupMenu();
    if (hMenu) {

        if (supListViewAddCopyValueItem(hMenu,
            Context->SearchList,
            ID_OBJECT_COPY,
            uPos,
            lpPoint,
            &Context->iSelectedItem,
            &Context->iColumnHit))
        {
            TrackPopupMenu(hMenu,
                TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                lpPoint->x,
                lpPoint->y,
                0,
                hwndDlg,
                NULL);
        }

        DestroyMenu(hMenu);
    }
}

/*
* FindDlgHandleSearch
*
* Purpose:
*
* Search button click handler.
*
*/
VOID FindDlgHandleSearch(
    _In_ HWND hwndDlg
)
{
    WCHAR           searchString[MAX_PATH + 1], typeName[MAX_PATH + 1];
    LPWSTR          pnameStr = (LPWSTR)searchString, ptypeStr = (LPWSTR)typeName;
    PFO_LIST_ITEM   flist, plist;
    ULONG           cci;

    supSetWaitCursor(TRUE);
    EnableWindow(GetDlgItem(hwndDlg, ID_SEARCH_FIND), FALSE);

    //
    // Update status bar.
    //
    _strcpy(searchString, TEXT("Searching..."));
    SetDlgItemText(hwndDlg, ID_SEARCH_STATUSBAR, searchString);

    ListView_DeleteAllItems(g_FindDlgContext.SearchList);

    RtlSecureZeroMemory(&searchString, sizeof(searchString));
    RtlSecureZeroMemory(&typeName, sizeof(typeName));

    GetDlgItemText(hwndDlg, ID_SEARCH_NAME, (LPWSTR)&searchString, MAX_PATH);
    GetDlgItemText(hwndDlg, ID_SEARCH_TYPE, (LPWSTR)&typeName, MAX_PATH);

    flist = NULL;

    if (searchString[0] == 0)
        pnameStr = NULL;
    if (typeName[0] == L'*')
        ptypeStr = 0;

    FindObject(KM_OBJECTS_ROOT_DIRECTORY, pnameStr, ptypeStr, &flist);

    //
    // Disable listview redraw
    //
    supListViewEnableRedraw(g_FindDlgContext.SearchList, FALSE);

    cci = 0;
    while (flist != NULL) {
        FindDlgAddListItem(g_FindDlgContext.SearchList, flist->ObjectName, flist->ObjectType);
        plist = flist->Prev;
        supHeapFree(flist);
        flist = plist;
        cci++;
    }

    //
    // Update status bar with results.
    //
    ultostr(cci, searchString);
    _strcat(searchString, TEXT(" matching object(s)."));
    SetDlgItemText(hwndDlg, ID_SEARCH_STATUSBAR, searchString);

    ListView_SortItemsEx(g_FindDlgContext.SearchList,
        &FindDlgCompareFunc, g_FindDlgContext.SortColumn);

    //
    // Enable listview redraw
    //
    supListViewEnableRedraw(g_FindDlgContext.SearchList, TRUE);

    supSetWaitCursor(FALSE);
    EnableWindow(GetDlgItem(hwndDlg, ID_SEARCH_FIND), TRUE);

}

/*
* FindDlgProc
*
* Purpose:
*
* Find Dialog window procedure.
*
*/
INT_PTR CALLBACK FindDlgProc(
    _In_  HWND hwndDlg,
    _In_  UINT uMsg,
    _In_  WPARAM wParam,
    _In_  LPARAM lParam
)
{
    if (uMsg == g_WinObj.SettingsChangeMessage) {
        FindDlgHandleSettingsChange(&g_FindDlgContext);
        return TRUE;
    }

    switch (uMsg) {

    case WM_NOTIFY:
        return FindDlgHandleNotify((LPNMLISTVIEW)lParam);

    case WM_GETMINMAXINFO:
        if (lParam) {
            supSetMinMaxTrackSize((PMINMAXINFO)lParam,
                FINDDLG_TRACKSIZE_MIN_X,
                FINDDLG_TRACKSIZE_MIN_Y,
                TRUE);
        }
        break;

    case WM_INITDIALOG:
        supCenterWindow(hwndDlg);
        FindDlgResize(hwndDlg, &g_FindDlgContext);
        break;

    case WM_SIZE:
        FindDlgResize(hwndDlg, &g_FindDlgContext);
        break;

    case WM_CLOSE:
        if (g_FindDlgContext.DialogIcon)
            DestroyIcon(g_FindDlgContext.DialogIcon);

        DestroyWindow(hwndDlg);
        g_WinObj.AuxDialogs[wobjFindDlgId] = NULL;
        break;

    case WM_COMMAND:

        switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case ID_OBJECT_COPY:

            supListViewCopyItemValueToClipboard(g_FindDlgContext.SearchList,
                g_FindDlgContext.iSelectedItem,
                g_FindDlgContext.iColumnHit);

            break;

        case IDCANCEL:
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            break;

        case ID_SEARCH_FIND:
            FindDlgHandleSearch(hwndDlg);
            break;

        default:
            break;
        }
        break;

    case WM_CONTEXTMENU:

        supHandleContextMenuMsgForListView(hwndDlg,
            wParam,
            lParam,
            g_FindDlgContext.SearchList,
            (pfnPopupMenuHandler)FindDlgHandlePopupMenu,
            &g_FindDlgContext);

        break;

    default:
        return FALSE;

    }

    return TRUE;
}

/*
* FindDlgAddTypes
*
* Purpose:
*
* Enumerate object types and fill combobox with them.
*
*/
VOID FindDlgAddTypes(
    _In_ HWND hwnd
)
{
    ULONG  i;
    HWND   hComboBox;
    SIZE_T sz;
    LPWSTR lpType;

    POBJECT_TYPE_INFORMATION  pObject;

    hComboBox = GetDlgItem(hwnd, ID_SEARCH_TYPE);
    if (hComboBox == NULL)
        return;

    SendMessage(hComboBox, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);

    if (g_pObjectTypesInfo == NULL) {
        SendMessage(hComboBox, CB_ADDSTRING, (WPARAM)0, (LPARAM)L"*");
        SendMessage(hComboBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        return;
    }

    __try {
        //
        // Type collection available, list it.
        //
        //
        // Warning: older Wine/Staging incorrectly implement memory structure layout for this structure and therefore will crash.            
        //
        pObject = OBJECT_TYPES_FIRST_ENTRY(g_pObjectTypesInfo);

        for (i = 0; i < g_pObjectTypesInfo->NumberOfTypes; i++) {
            sz = pObject->TypeName.MaximumLength + sizeof(UNICODE_NULL);
            lpType = (LPWSTR)supHeapAlloc(sz);
            if (lpType) {

                _strncpy(lpType,
                    sz / sizeof(WCHAR),
                    pObject->TypeName.Buffer,
                    pObject->TypeName.Length / sizeof(WCHAR));

                SendMessage(hComboBox, CB_ADDSTRING, (WPARAM)0, (LPARAM)lpType);
                supHeapFree(lpType);
            }
            pObject = OBJECT_TYPES_NEXT_ENTRY(pObject);
        }
    }
    __finally {
        SendMessage(hComboBox, CB_ADDSTRING, (WPARAM)0, (LPARAM)L"*");
        SendMessage(hComboBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        if (AbnormalTermination())
            supReportAbnormalTermination(__FUNCTIONW__);
    }
}

/*
* FindDlgCreate
*
* Purpose:
*
* Create and initialize Find Dialog.
*
*/
VOID FindDlgCreate(
    _In_ HWND hwndParent
)
{
    HWND hwndDlg;
    INT iImage = ImageList_GetImageCount(g_ListViewImages) - 1;
    LVCOLUMNS_DATA columnData[] =
    {
        { L"Name", 300, LVCFMT_LEFT | LVCFMT_BITMAP_ON_RIGHT,  iImage },
        { L"Type", 100, LVCFMT_LEFT | LVCFMT_BITMAP_ON_RIGHT,  I_IMAGENONE }
    };


    //
    // Allow only one search dialog per time.
    //
    ENSURE_DIALOG_UNIQUE(g_WinObj.AuxDialogs[wobjFindDlgId]);

    RtlSecureZeroMemory(&g_FindDlgContext, sizeof(g_FindDlgContext));

    hwndDlg = CreateDialogParam(g_WinObj.hInstance, 
        MAKEINTRESOURCE(IDD_DIALOG_SEARCH), hwndParent, &FindDlgProc, 0);

    if (hwndDlg == NULL)
        return;

    g_WinObj.AuxDialogs[wobjFindDlgId] = hwndDlg;
    g_FindDlgContext.DialogWindow = hwndDlg;
    g_FindDlgContext.StatusBar = GetDlgItem(hwndDlg, ID_SEARCH_STATUSBAR);
    g_FindDlgContext.iColumnHit = -1;
    g_FindDlgContext.iSelectedItem = -1;

    //
    // Set dialog icon.
    //
    g_FindDlgContext.DialogIcon = (HICON)LoadImage(g_WinObj.hInstance,
        MAKEINTRESOURCE(IDI_ICON_MAIN),
        IMAGE_ICON,
        32, 32,
        0);

    if (g_FindDlgContext.DialogIcon) {
        SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_FindDlgContext.DialogIcon);
        SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)g_FindDlgContext.DialogIcon);
    }

    g_FindDlgContext.SearchList = GetDlgItem(hwndDlg, ID_SEARCH_LIST);
    if (g_FindDlgContext.SearchList) {

        //
        // Set listview imagelist, style flags and theme.
        //
        supSetListViewSettings(g_FindDlgContext.SearchList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP,
            FALSE,
            TRUE,
            g_ListViewImages,
            LVSIL_SMALL);

        //
        // And columns and remember their count.
        //
        g_FindDlgContext.ColumnCount = supAddLVColumnsFromArray(
            g_FindDlgContext.SearchList,
            columnData,
            RTL_NUMBER_OF(columnData));

    }
    FindDlgAddTypes(hwndDlg);
}
