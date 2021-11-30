// dear imgui
// (test engine, test context = end user automation API)

#pragma once

#include "imgui.h"
#include "imgui_te_engine.h"    // IM_CHECK*, various flags, enums
#include <stdint.h>             // intptr_t

// Undo some of the damage done by <windows.h>
#ifdef Yield
#undef Yield
#endif

//-------------------------------------------------------------------------
// External forward declaration
//-------------------------------------------------------------------------

struct ImGuiCaptureArgs;        // Parameters for ctx->CaptureXXX functions
struct ImGuiTest;               // A test registered with IM_REGISTER_TEST()
struct ImGuiTestEngine;         // Test Engine Instance (opaque)
struct ImGuiTestEngineIO;       // Test Engine IO structure (configuration flags, state)
struct ImGuiTestItemInfo;       // Information gathered about an item: label, status, position etc.
struct ImGuiTestInputs;         // Test Engine Simulated Inputs structure (opaque)
struct ImGuiTestGatherTask;     // Test Engine task for scanning/finding items

//-------------------------------------------------------------------------
// ImGuiTestRef
//-------------------------------------------------------------------------

// Weak reference to an Item/Window given an hashed ID or a string path ID.
struct ImGuiTestRef
{
    ImGuiID         ID;
    const char*     Path;

    ImGuiTestRef()                  { ID = 0; Path = NULL; }
    ImGuiTestRef(ImGuiID id)        { ID = id; Path = NULL; }
    ImGuiTestRef(const char* p)     { ID = 0; Path = p; }
    bool IsEmpty() const            { return ID == 0 && (Path == NULL || Path[0] == 0); }
};

// Helper to output a string showing the Path, ID or Debug Label based on what is available (some items only have ID as we couldn't find/store a Path)
struct ImGuiTestRefDesc
{
    char            Buf[80];

    const char* c_str()             { return Buf; }
    ImGuiTestRefDesc(const ImGuiTestRef& ref, const ImGuiTestItemInfo* item = NULL);
};

//-------------------------------------------------------------------------
// ImGuiTestContext
// This is the interface that most tests will interact with.
//-------------------------------------------------------------------------

enum ImGuiTestAction
{
    ImGuiTestAction_Unknown = 0,
    ImGuiTestAction_Hover,          // Move mouse
    ImGuiTestAction_Click,          // Move mouse and click
    ImGuiTestAction_DoubleClick,    // Move mouse and double-click
    ImGuiTestAction_Check,          // Check item if unchecked (Checkbox, MenuItem or any widget reporting ImGuiItemStatusFlags_Checkable)
    ImGuiTestAction_Uncheck,        // Uncheck item if checked
    ImGuiTestAction_Open,           // Open item if closed (TreeNode, BeginMenu or any widget reporting ImGuiItemStatusFlags_Openable)
    ImGuiTestAction_Close,          // Close item if opened
    ImGuiTestAction_Input,          // Start text inputing into a field (e.g. CTRL+Click on Drags/Slider, click on InputText etc.)
    ImGuiTestAction_NavActivate,    // Activate item with navigation
    ImGuiTestAction_COUNT
};

struct ImGuiTestActionFilter
{
    int                     MaxDepth;
    int                     MaxPasses;
    const int*              MaxItemCountPerDepth;
    ImGuiItemStatusFlags    RequireAllStatusFlags;
    ImGuiItemStatusFlags    RequireAnyStatusFlags;

    ImGuiTestActionFilter() { MaxDepth = -1; MaxPasses = -1; MaxItemCountPerDepth = NULL; RequireAllStatusFlags = RequireAnyStatusFlags = 0; }
};

// Helper struct to store various query-able state of an item.
// This facilitate interactions between GuiFunc and TestFunc, since those state are frequently used.
struct ImGuiTestGenericItemStatus
{
    int     Ret;            // return value
    int     Hovered;        // result of IsItemHovered()
    int     Active;         // result of IsItemActive()
    int     Focused;        // result of IsItemFocused()
    int     Clicked;        // result of IsItemClicked()
    int     Visible;        // result of IsItemVisible()
    int     Edited;         // result of IsItemEdited()
    int     Activated;      // result of IsItemActivated()
    int     Deactivated;    // result of IsItemDeactivated()
    int     DeactivatedAfterEdit;//.. of IsItemDeactivatedAfterEdit()

    ImGuiTestGenericItemStatus()        { Clear(); }
    void Clear()                        { memset(this, 0, sizeof(*this)); }
    void QuerySet(bool ret_val = false) { Clear(); QueryInc(ret_val); }
    void QueryInc(bool ret_val = false) { Ret += ret_val; Hovered += ImGui::IsItemHovered(); Active += ImGui::IsItemActive(); Focused += ImGui::IsItemFocused(); Clicked += ImGui::IsItemClicked(); Visible += ImGui::IsItemVisible(); Edited += ImGui::IsItemEdited(); Activated += ImGui::IsItemActivated(); Deactivated += ImGui::IsItemDeactivated(); DeactivatedAfterEdit += ImGui::IsItemDeactivatedAfterEdit(); }
};

// Generic structure with various storage fields.
// This is useful for tests to quickly share data between GuiFunc and TestFunc.
// If those fields are not enough: using ctx->SetVarsDataType<>() and ctx->GetVars<>() it is possible to store custom data on the stack.
struct ImGuiTestGenericVars
{
    // Generic storage with a bit of semantic to make user/test code look neater
    int                     Step;
    int                     Count;
    ImGuiID                 DockId;
    ImGuiWindowFlags        WindowFlags;
    ImGuiTableFlags         TableFlags;
    ImGuiOldColumnFlags     ColumnsFlags;
    ImGuiTestGenericItemStatus  Status;
    bool                    ShowWindows;
    bool                    UseClipper;
    bool                    UseViewports;
    float                   Width;
    ImVec2                  Pos;
    ImVec2                  Size;
    ImVec2                  Pivot;
    ImVec4                  Color1, Color2;

    // Generic storage
    int                     Int1, Int2;
    int                     IntArray[10];
    float                   Float1, Float2;
    float                   FloatArray[10];
    bool                    Bool1, Bool2;
    bool                    BoolArray[10];
    ImGuiID                 Id;
    ImGuiID                 IdArray[10];
    char                    Str1[256];
    char                    Str2[256];
    ImVector<char>          StrLarge;

    ImGuiTestGenericVars()  { Clear(); }
    void Clear()            { StrLarge.clear(); memset(this, 0, sizeof(*this)); }
};

enum ImGuiTestActiveFunc
{
    ImGuiTestActiveFunc_None,
    ImGuiTestActiveFunc_GuiFunc,
    ImGuiTestActiveFunc_TestFunc
};

// Context for a running ImGuiTest
struct ImGuiTestContext
{
    // [Internal Fields]
    ImGuiTestEngine*        Engine = NULL;
    ImGuiTest*              Test = NULL;
    ImGuiTestEngineIO*      EngineIO = NULL;
    ImGuiContext*           UiContext = NULL;
    ImGuiTestInputs*        Inputs = NULL;
    ImGuiTestGatherTask*    GatherTask = NULL;
    ImGuiTestRunFlags       RunFlags = ImGuiTestRunFlags_None;
    ImGuiTestActiveFunc     ActiveFunc = ImGuiTestActiveFunc_None;  // None/GuiFunc/TestFunc
    void*                   UserData = NULL;
    int                     FrameCount = 0;                         // Test frame count (restarts from zero every time)
    int                     FirstTestFrameCount = 0;                // First frame where TestFunc is running (after warm-up frame). This is generally -1 or 0 depending on whether we have warm up enabled
    double                  RunningTime = 0.0f;                     // Amount of wall clock time the Test has been running. Used by safety watchdog.
    ImU64                   BatchStartTime = 0;
    int                     ActionDepth = 0;
    int                     CaptureCounter = 0;
    int                     ErrorCounter = 0;
    bool                    FirstGuiFrame = false;
    bool                    Abort = false;
    bool                    HasDock = false;                        // #ifdef IMGUI_HAS_DOCK expressed in an easier to test value
    double                  PerfRefDt = -1.0;
    int                     PerfStressAmount = 0;                   // Convenience copy of engine->IO.PerfStressAmount

    // Commonly user exposed state for the ctx-> functions
    ImGuiTestGenericVars    GenericVars;
    char                    RefStr[256] = { 0 };                    // Reference window/path for ID construction
    ImGuiID                 RefID = 0;
    ImGuiInputSource        InputMode = ImGuiInputSource_Mouse;
    ImGuiTestOpFlags        OpFlags = ImGuiTestOpFlags_None;        // Supported: ImGuiTestOpFlags_NoAutoUncollapse
    ImVector<char>          Clipboard;
    ImVector<ImGuiWindow*>  ForeignWindowsToHide;

    // Main control
    void        Finish();
    bool        IsError() const             { return Test->Status == ImGuiTestStatus_Error || Abort; }
    bool        IsFirstGuiFrame() const     { return FirstGuiFrame; }
    bool        IsFirstTestFrame() const    { return FrameCount == FirstTestFrameCount; }   // First frame where TestFunc is running (after warm-up frame).
    bool        IsGuiFuncOnly() const       { return (RunFlags & ImGuiTestRunFlags_GuiFuncOnly) != 0; }
    void        SetGuiFuncEnabled(bool v)   { if (v) RunFlags &= ~ImGuiTestRunFlags_GuiFuncDisable; else RunFlags |= ImGuiTestRunFlags_GuiFuncDisable; }
    void        RecoverFromUiContextErrors();
    template <typename T> T& GetVars()      { IM_ASSERT(UserData != NULL); return *(T*)(UserData); } // Campanion to using t->SetVarsDataType<>(). FIXME: Assert to compare sizes

    // Debug Control Flow
    bool        DebugHaltTestFunc(const char* file, int line);

    // Logging
    void        LogEx(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, ...) IM_FMTARGS(4);
    void        LogExV(ImGuiTestVerboseLevel level, ImGuiTestLogFlags flags, const char* fmt, va_list args) IM_FMTLIST(4);
    void        LogToTTY(ImGuiTestVerboseLevel level, const char* message);
    void        LogToDebugger(ImGuiTestVerboseLevel level, const char* message);
    void        LogDebug(const char* fmt, ...)      IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Debug or ImGuiTestVerboseLevel_Trace depending on context depth
    void        LogInfo(const char* fmt, ...)       IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Info
    void        LogWarning(const char* fmt, ...)    IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Warning
    void        LogError(const char* fmt, ...)      IM_FMTARGS(2);  // ImGuiTestVerboseLevel_Error
    void        LogBasicUiState();

    // Yield, Timing
    void        Yield(int count = 1);
    void        YieldUntil(int frame_count);
    void        Sleep(float time);
    void        SleepNoSkip(float time, float frame_time_step);
    void        SleepShort();

    // Windows
    // FIXME-TESTS: Refactor this horrible mess... perhaps all functions should have a ImGuiTestRef defaulting to empty?
    void        SetRef(ImGuiTestRef ref);
    void        SetRef(ImGuiWindow* window); // Shortcut to SetRef(window->Name) which works for ChildWindow (see code)
    ImGuiTestRef GetRef();
    void        WindowClose(ImGuiTestRef ref);
    void        WindowCollapse(ImGuiWindow* window, bool collapsed);
    void        WindowFocus(ImGuiTestRef ref);
    void        WindowMove(ImGuiTestRef ref, ImVec2 pos, ImVec2 pivot = ImVec2(0.0f, 0.0f), ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        WindowResize(ImGuiTestRef ref, ImVec2 sz);
    bool        WindowTeleportToMakePosVisibleInViewport(ImGuiWindow* window, ImVec2 pos_in_window);
    bool        WindowBringToFront(ImGuiWindow* window, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        PopupCloseOne();
    void        PopupCloseAll();
    ImGuiWindow* GetWindowByRef(ImGuiTestRef ref);
    ImGuiTestRef GetFocusWindowRef();

    void        ForeignWindowsHideOverPos(ImVec2 pos, ImGuiWindow** ignore_list);
    void        ForeignWindowsUnhideAll();

    // ID
    ImGuiID     GetID(ImGuiTestRef ref);
    ImGuiID     GetID(ImGuiTestRef ref, ImGuiTestRef seed_ref);
    ImGuiID     GetIDByInt(int n);
    ImGuiID     GetIDByInt(int n, ImGuiTestRef seed_ref);
    ImGuiID     GetIDByPtr(void* p);
    ImGuiID     GetIDByPtr(void* p, ImGuiTestRef seed_ref);
    // FIXME: too many overload confusing, remove the two first one, let user pass ""
    ImGuiID     GetChildWindowID(const char* child_name);                           // Name created by BeginChild("name", ...), using current Ref as parent.
    ImGuiID     GetChildWindowID(ImGuiID child_id);                                 // Name created by BeginChild(id, ...), using current Ref as parent.
    ImGuiID     GetChildWindowID(ImGuiTestRef parent_ref, const char* child_name);  // Name created by BeginChild("name", ...), using specified parent.
    ImGuiID     GetChildWindowID(ImGuiTestRef parent_ref, ImGuiID child_id);        // Name created by BeginChild(id, ...), using specified parent.

    // Misc
    ImVec2      GetMainViewportPos();
    ImVec2      GetMainViewportSize();
    ImVec2      GetVoidPos();
    ImVec2      GetWindowTitlebarPoint(ImGuiTestRef window_ref);                    // Return a clickable point on window titlebar (window tab for docked windows).

    // Screen captures
    // - Simple API
    void        CaptureScreenshotWindow(ImGuiTestRef ref, int capture_flags = 0);
    // - Advanced API
    void        CaptureInitArgs(ImGuiCaptureArgs* args, int capture_flags = 0);
    bool        CaptureAddWindow(ImGuiCaptureArgs* args, ImGuiTestRef ref);
    bool        CaptureScreenshotEx(ImGuiCaptureArgs* args);
    // - Animation capturing API
    bool        BeginCaptureGif(ImGuiCaptureArgs* args);
    bool        EndCaptureGif(ImGuiCaptureArgs* args);

    // Mouse inputs
    void        MouseMove(ImGuiTestRef ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void        MouseMoveToPos(ImVec2 pos);
    void        MouseTeleportToPos(ImVec2 pos);
    void        MouseClick(ImGuiMouseButton button = 0);
    void        MouseClickMulti(ImGuiMouseButton button, int count);
    void        MouseDoubleClick(ImGuiMouseButton button = 0);
    void        MouseDown(ImGuiMouseButton button = 0);
    void        MouseUp(ImGuiMouseButton button = 0);
    void        MouseLiftDragThreshold(ImGuiMouseButton button = 0);
    void        MouseClickOnVoid(ImGuiMouseButton button = 0);
    void        MouseDragWithDelta(ImVec2 delta, ImGuiMouseButton button = 0);
    void        MouseWheel(float vertical, float horizontal=0.0f);
    void        MouseWheel(ImVec2 delta);
    bool        FindExistingVoidPosOnViewport(ImGuiViewport* viewport, ImVec2* out);

    // Keyboard inputs
    void        KeyDownMap(ImGuiKey key, int mod_flags = 0);
    void        KeyUpMap(ImGuiKey key, int mod_flags = 0);
    void        KeyPressMap(ImGuiKey key, int mod_flags = 0, int count = 1);
    void        KeyHoldMap(ImGuiKey key, int mod_flags, float time);
    void        KeyChars(const char* chars);
    void        KeyCharsAppend(const char* chars);
    void        KeyCharsAppendEnter(const char* chars);
    void        KeyCharsReplace(const char* chars);
    void        KeyCharsReplaceEnter(const char* chars);

    // Navigation inputs
    void        SetInputMode(ImGuiInputSource input_mode);
    void        NavKeyPress(ImGuiNavInput input);
    void        NavKeyDown(ImGuiNavInput input);
    void        NavKeyUp(ImGuiNavInput input);
    void        NavMoveTo(ImGuiTestRef ref);
    void        NavActivate();  // Activate current selected item. Same as pressing [space].
    void        NavInput();     // Press ImGuiNavInput_Input (e.g. Triangle) to turn a widget into a text input
    void        NavEnableForWindow();

    // Scrolling
    void        ScrollTo(ImGuiWindow* window, ImGuiAxis axis, float scroll_v);
    void        ScrollToX(float scroll_x) { ScrollTo(GetWindowByRef(""), ImGuiAxis_X, scroll_x); }
    void        ScrollToY(float scroll_y) { ScrollTo(GetWindowByRef(""), ImGuiAxis_Y, scroll_y); }
    void        ScrollToTop();
    void        ScrollToBottom();
    void        ScrollToItemY(ImGuiTestRef ref, float scroll_ratio_y = 0.5f);
    void        ScrollToItemX(ImGuiTestRef ref);
    bool        ScrollErrorCheck(ImGuiAxis axis, float expected, float actual, int* remaining_attempts);
    void        ScrollVerifyScrollMax(ImGuiWindow* window);

    // Low-level queries
    ImGuiTestItemInfo*  ItemInfo(ImGuiTestRef ref, ImGuiTestOpFlags flags = ImGuiTestOpFlags_None);
    void                GatherItems(ImGuiTestItemList* out_list, ImGuiTestRef parent, int depth = -1);

    // Item/Widgets manipulation
    void        ItemAction(ImGuiTestAction action, ImGuiTestRef ref, void* action_arg = NULL, ImGuiTestOpFlags flags = 0);
    void        ItemClick(ImGuiTestRef ref, ImGuiMouseButton button = 0, ImGuiTestOpFlags flags = 0) { ItemAction(ImGuiTestAction_Click, ref, (void*)(intptr_t)button, flags); }
    void        ItemDoubleClick(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)           { ItemAction(ImGuiTestAction_DoubleClick, ref, NULL, flags); }
    void        ItemCheck(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Check, ref, NULL, flags); }
    void        ItemUncheck(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)               { ItemAction(ImGuiTestAction_Uncheck, ref, NULL, flags); }
    void        ItemOpen(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                  { ItemAction(ImGuiTestAction_Open, ref, NULL, flags); }
    void        ItemClose(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Close, ref, NULL, flags); }
    void        ItemInput(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)                 { ItemAction(ImGuiTestAction_Input, ref, NULL, flags); }
    void        ItemNavActivate(ImGuiTestRef ref, ImGuiTestOpFlags flags = 0)           { ItemAction(ImGuiTestAction_NavActivate, ref, NULL, flags); }

    // Item/Widgets: Batch actions over an entire scope
    void        ItemActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent, const ImGuiTestActionFilter* filter = NULL);
    void        ItemOpenAll(ImGuiTestRef ref_parent, int depth = -1, int passes = -1);
    void        ItemCloseAll(ImGuiTestRef ref_parent, int depth = -1, int passes = -1);

    // Item/Widgets: Helpers to easily set a value
    void        ItemInputValue(ImGuiTestRef ref, int v);
    void        ItemInputValue(ImGuiTestRef ref, float f);
    void        ItemInputValue(ImGuiTestRef ref, const char* str);

    // Item/Widgets: Drag and Mouse operations
    void        ItemHold(ImGuiTestRef ref, float time);
    void        ItemHoldForFrames(ImGuiTestRef ref, int frames);
    void        ItemDragOverAndHold(ImGuiTestRef ref_src, ImGuiTestRef ref_dst);
    void        ItemDragAndDrop(ImGuiTestRef ref_src, ImGuiTestRef ref_dst, ImGuiMouseButton button = 0);
    void        ItemDragWithDelta(ImGuiTestRef ref_src, ImVec2 pos_delta);
    void        ItemVerifyCheckedIfAlive(ImGuiTestRef ref, bool checked);

    // Helpers for Tab Bars widgets
    void        TabClose(ImGuiTestRef ref);
    bool        TabBarCompareOrder(ImGuiTabBar* tab_bar, const char** tab_order);

    // Helpers for Menus widgets
    void        MenuAction(ImGuiTestAction action, ImGuiTestRef ref);
    void        MenuActionAll(ImGuiTestAction action, ImGuiTestRef ref_parent);
    void        MenuClick(ImGuiTestRef ref)                 { MenuAction(ImGuiTestAction_Click, ref); }
    void        MenuCheck(ImGuiTestRef ref)                 { MenuAction(ImGuiTestAction_Check, ref); }
    void        MenuUncheck(ImGuiTestRef ref)               { MenuAction(ImGuiTestAction_Uncheck, ref); }
    void        MenuCheckAll(ImGuiTestRef ref_parent)       { MenuActionAll(ImGuiTestAction_Check, ref_parent); }
    void        MenuUncheckAll(ImGuiTestRef ref_parent)     { MenuActionAll(ImGuiTestAction_Uncheck, ref_parent); }

    // Helpers for Combo Boxes
    void        ComboClick(ImGuiTestRef ref);
    void        ComboClickAll(ImGuiTestRef ref);

    // Helpers for Tables
    void                        TableOpenContextMenu(ImGuiTestRef ref, int column_n = -1);
    ImGuiSortDirection          TableClickHeader(ImGuiTestRef ref, const char* label, ImGuiKeyModFlags keys_mod = ImGuiKeyModFlags_None);
    void                        TableSetColumnEnabled(ImGuiTestRef ref, const char* label, bool enabled);
    void                        TableResizeColumn(ImGuiTestRef ref, int column_n, float width);
    const ImGuiTableSortSpecs*  TableGetSortSpecs(ImGuiTestRef ref);

    // Docking
#ifdef IMGUI_HAS_DOCK
    void        DockClear(const char* window_name, ...);
    void        DockInto(ImGuiTestRef src_id, ImGuiTestRef dst_id, ImGuiDir split_dir = ImGuiDir_None, bool is_outer_docking = false);
    void        UndockNode(ImGuiID dock_id);
    void        UndockWindow(const char* window_name);

    bool        WindowIsUndockedOrStandalone(ImGuiWindow* window);
    bool        DockIdIsUndockedOrStandalone(ImGuiID dock_id);
    void        DockNodeHideTabBar(ImGuiDockNode* node, bool hidden);
#endif

    // Performances
    void        PerfCalcRef();
    void        PerfCapture(const char* category = NULL, const char* test_name = NULL, const char* csv_file = NULL);
};
