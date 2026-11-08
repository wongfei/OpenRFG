// Original: library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp
//
// The Windows platform part of the framework: WinMain (the process entry), the crash handler (a minidump next to the
// exe, then the Backtrace upload), the window (class, log file, creation, the aspect ratio constraint of its window
// procedure), the message pump, the display (keen graphics on the window) and the platform queries.
//
// Deviation: the framework's message boxes are
// commented out and replaced by log lines; their string table text is empty anyway, because rfg.exe's string table is
// not linked in (gameframework.cpp's loadFrameworkMessage, handleFatalError).

#include "keen/gameframework/gameframework.hpp"
#include "game/build/win32/steam/master/generated_sources/version.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/trace.hpp"
#include "keen/base/memorysystem.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframework_platform.hpp"
#include "library/modules/runtime/gameframework/sources/global/gameframeworksystem.hpp"
#include "library/modules/runtime/gameframework/sources/win32/backtraceuploader.hpp"
#include "library/modules/runtime/gameframework/sources/win32/gameframework_win32.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"

#include <float.h>
#include <stdlib.h>
#include <windows.h>
#include <dbghelp.h>
#include <strsafe.h>

namespace keen
{
    HINSTANCE__* g_hInstance; // 0x038B6BC0
    HINSTANCE__* g_prevInstance; // 0x038B4F58

    static WindowAspectRatioConstraintData s_windowAspectRatioConstraintData; // 0x038B6BE4
    static void (*s_pCustomCrashDumpCallback)(const char*, const char*); // 0x038B6BE0
    static bool s_overrideSystemLanguage; // 0x038B6BD8 (only read: always false)
    static LanguageId s_systemLanguage; // 0x038B6BDC
}

// The crash handler's state: the log file's path (initializeBase), the paths generateDump builds (UTF-16).
static keen::LanguageFamily s_languageFamily; // 0x038B4F5C
static WCHAR s_logFileName[260]; // 0x038B63B0
static WCHAR s_dumpFileName[260]; // 0x038B5780
static WCHAR s_exeFileName[260]; // 0x038B65B8
static WCHAR s_pdbFileName[260]; // 0x038B4D50
static WCHAR s_zipFileName[260]; // 0x038B5988
static WCHAR s_directoryWithPrefixUtf16[520]; // 0x038B5B90
static WCHAR s_filenameUtf16[520]; // 0x038B5370
static WCHAR s_completePathUtf16[520]; // 0x038B5FA0
static WCHAR s_buildProjectNameUtf16[520]; // 0x038B4F60

namespace keen
{
    // 0x00C0E980
    // Keeps the client area's aspect ratio (width / height) between the constraint's limits:
    //   - WM_SIZING: the rectangle being dragged (lParam) is corrected at the dragged edge; for a corner, the side
    //     edge when the ratio was clamped to the minimum, the top/bottom edge when it was clamped to the maximum;
    //   - WM_WINDOWPOSCHANGED (a size change): the window is resized with SetWindowPos.
    // Returns whether the message was handled. The ratios are computed in double and rounded to float; the corrected
    // sizes are truncated.
    // Original behaviour kept: WM_WINDOWPOSCHANGED is handled for any size change, even the one its own SetWindowPos
    // makes (that one already has the ratio, so it is not handled again).
    static bool constrainWindowGeometry(HWND hwnd, uint32 message, uint32 wParam, long lParam, const WindowAspectRatioConstraintData* pData)
    {
        if (message == WM_WINDOWPOSCHANGED)
        {
            WINDOWPOS* pPosition = (WINDOWPOS*)lParam;
            if ((pPosition->flags & SWP_NOSIZE) != 0)
            {
                return false;
            }
            RECT windowRect;
            RECT clientRect;
            GetWindowRect(hwnd, &windowRect);
            GetClientRect(hwnd, &clientRect);
            int clientHeight = clientRect.bottom;
            if (clientHeight == 0)
            {
                return false;
            }
            int clientWidth = clientRect.right;
            const int borderWidth = windowRect.right - clientRect.right - windowRect.left;
            const int borderHeight = windowRect.bottom - windowRect.top - clientRect.bottom;
            const float width = (float)clientWidth;
            const float height = (float)clientHeight;
            const float aspectRatio = (float)((double)width / (double)height);
            if ((double)pData->minAspectRatio > (double)aspectRatio)
            {
                clientWidth = (int)((double)height * (double)pData->minAspectRatio);
            }
            else if ((double)aspectRatio > (double)pData->maxAspectRatio)
            {
                clientHeight = (int)((double)width / (double)pData->maxAspectRatio);
            }
            else
            {
                return false;
            }
            // Magic number: SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS.
            SetWindowPos(hwnd, nullptr, pPosition->x, pPosition->y, clientWidth + borderWidth, borderHeight + clientHeight, 0x144);
            return true;
        }
        if (message != WM_SIZING)
        {
            return false;
        }
        RECT* pRect = (RECT*)lParam;
        RECT windowRect;
        RECT clientRect;
        GetWindowRect(hwnd, &windowRect);
        GetClientRect(hwnd, &clientRect);
        const int borderWidth = windowRect.right - clientRect.right - windowRect.left;
        const int borderHeight = windowRect.bottom - clientRect.bottom - windowRect.top;
        const float width = (float)(pRect->right - pRect->left - borderWidth);
        const float height = (float)(pRect->bottom - pRect->top - borderHeight);
        const float aspectRatio = (float)((double)width / (double)height);
        float constrained = aspectRatio;
        if ((double)pData->minAspectRatio > (double)aspectRatio)
        {
            constrained = pData->minAspectRatio;
        }
        else if ((double)aspectRatio > (double)pData->maxAspectRatio)
        {
            constrained = pData->maxAspectRatio;
        }
        if ((double)constrained == (double)aspectRatio)
        {
            return false;
        }
        const bool clampedToMaximum = (double)constrained == (double)pData->maxAspectRatio;
        const int widthForHeight = (int)((double)height * (double)constrained);
        const int heightForWidth = (int)((double)width / (double)constrained);
        switch (wParam)
        {
        case WMSZ_LEFT:
            pRect->left = pRect->right - widthForHeight - borderWidth;
            break;
        case WMSZ_RIGHT:
            pRect->right = widthForHeight + pRect->left + borderWidth;
            break;
        case WMSZ_TOP:
            pRect->top = pRect->bottom - heightForWidth - borderHeight;
            break;
        case WMSZ_BOTTOM:
            pRect->bottom = heightForWidth + pRect->top + borderHeight;
            break;
        case WMSZ_TOPLEFT:
            if (clampedToMaximum)
            {
                pRect->top = pRect->bottom - heightForWidth - borderHeight;
            }
            else
            {
                pRect->left = pRect->right - widthForHeight - borderWidth;
            }
            break;
        case WMSZ_TOPRIGHT:
            if (clampedToMaximum)
            {
                pRect->top = pRect->bottom - heightForWidth - borderHeight;
            }
            else
            {
                pRect->right = widthForHeight + pRect->left + borderWidth;
            }
            break;
        case WMSZ_BOTTOMLEFT:
            if (clampedToMaximum)
            {
                pRect->bottom = heightForWidth + pRect->top + borderHeight;
            }
            else
            {
                pRect->left = pRect->right - widthForHeight - borderWidth;
            }
            break;
        case WMSZ_BOTTOMRIGHT:
            if (clampedToMaximum)
            {
                pRect->bottom = heightForWidth + pRect->top + borderHeight;
            }
            else
            {
                pRect->right = widthForHeight + pRect->left + borderWidth;
            }
            break;
        default:
            break;
        }
        return true;
    }

    // 0x00C0ED50
    // Alt+Enter (SC_KEYMENU with the Enter key) is counted for updateBase, which toggles the window mode; the window
    // keeps its GameFrameworkSystem in its window long 0. Closing the window quits (WM_QUIT through handleMessages).
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_SYSCOMMAND)
        {
            if ((wParam & 0xFFF0u) == SC_KEYMENU)
            {
                if (lParam == VK_RETURN)
                {
                    GameFrameworkSystem* pGameFramework = (GameFrameworkSystem*)GetWindowLongW(hwnd, 0);
                    if (pGameFramework != nullptr)
                    {
                        pGameFramework->triggerAltEnter++;
                    }
                }
                return 0;
            }
        }
        else if (uMsg == WM_DESTROY)
        {
            PostQuitMessage(0);
        }
        else if (uMsg == WM_WINDOWPOSCHANGED)
        {
            if (s_windowAspectRatioConstraintData.isConstrained && constrainWindowGeometry(hwnd, WM_WINDOWPOSCHANGED, (uint32)wParam, (long)lParam, &s_windowAspectRatioConstraintData))
            {
                return 0;
            }
        }
        else if (uMsg == WM_SIZING)
        {
            if (s_windowAspectRatioConstraintData.isConstrained && constrainWindowGeometry(hwnd, WM_SIZING, (uint32)wParam, (long)lParam, &s_windowAspectRatioConstraintData))
            {
                return 1;
            }
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    // 0x00C0EE20
    static void handleMessages(GameFrameworkSystem* pGameFramework)
    {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            if (message.message == WM_QUIT)
            {
                pGameFramework->quit = true;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    // 0x00C0FCD0
    // <exe dir><exe name>.dmp (the exception, the data segments) and the paths the Backtrace upload would send.
    static bool generateDump(EXCEPTION_POINTERS* pExceptionPointers)
    {
        SYSTEMTIME localTime;
        GetLocalTime(&localTime);
        WCHAR exePathUtf16[260];
        GetModuleFileNameW(nullptr, exePathUtf16, 0x104);
        char exePath[520];
        convertUTF16ToUTF8(exePath, sizeof(exePath), (const uint16*)exePathUtf16);
        FilePath exeFilePath(exePath, false);
        convertUTF8ToUTF16((uint16*)s_directoryWithPrefixUtf16, sizeof(s_directoryWithPrefixUtf16), exeFilePath.getDirectoryWithPrefix());
        convertUTF8ToUTF16((uint16*)s_filenameUtf16, sizeof(s_filenameUtf16), exeFilePath.getFileName());
        convertUTF8ToUTF16((uint16*)s_completePathUtf16, sizeof(s_completePathUtf16), exeFilePath.getCompletePath());
        convertUTF8ToUTF16((uint16*)s_buildProjectNameUtf16, sizeof(s_buildProjectNameUtf16), getBuildProjectName());
        StringCchPrintfW(s_dumpFileName, 0x104, L"%s%s.dmp", s_directoryWithPrefixUtf16, s_filenameUtf16);
        StringCchPrintfW(s_exeFileName, 0x104, L"%s", s_completePathUtf16);
        StringCchPrintfW(s_pdbFileName, 0x104, L"%s%s.pdb", s_directoryWithPrefixUtf16, s_filenameUtf16);
        BOOL written = FALSE;
        HANDLE hFile = CreateFileW(s_dumpFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            char version[64];
            copyString(version, sizeof(version), getBuildVersionString());
            replaceStringCharacters(version, ":/\\\"?* ", '_');
            WCHAR versionUtf16[128];
            convertUTF8ToUTF16((uint16*)versionUtf16, sizeof(versionUtf16), version);
            StringCchPrintfW(s_zipFileName, 0x104, L"%s%s-%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.zip", s_directoryWithPrefixUtf16,
                             s_buildProjectNameUtf16, versionUtf16, localTime.wYear, localTime.wMonth, localTime.wDay, localTime.wHour,
                             localTime.wMinute, localTime.wSecond, GetCurrentProcessId(), GetCurrentThreadId());
            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation;
            exceptionInformation.ThreadId = GetCurrentThreadId();
            exceptionInformation.ExceptionPointers = pExceptionPointers;
            exceptionInformation.ClientPointers = TRUE;
            // Magic number: MiniDumpWithDataSegs | MiniDumpIgnoreInaccessibleMemory.
            written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)0x20001, &exceptionInformation, nullptr, nullptr);
            CloseHandle(hFile);
        }
        return written == TRUE;
    }
}

// 0x00C0FF20
// Asks whether to write a crash report; yes writes the minidump, closes the log and uploads both (deviation, orig_main.md
// 3.4: the upload sends nothing).
static void handleFatalError(EXCEPTION_POINTERS* pExceptionPointers)
{
    WCHAR message[4048];
    // Magic number: the first string table id of the crash question (one entry per language family, as the framework's
    // other messages). Deviation: the buffer is emptied first (no string table, see the file comment).
    static const UINT messageIds[15] = {0x119, 0x11A, 0x11B, 0x11C, 0x11D, 0x11F, 0x11E, 0x120, 0x121, 0x122, 0x123, 0x11D, 0x124, 0x124, 0x125};
    message[0] = L'\0';
    LoadStringW(nullptr, messageIds[s_languageFamily], message, 0xFD0);
    // OpenRFG deviation: no question box; the crash is logged and the dump is always written (the "yes" answer).
    // Magic number: MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND.
    // if (MessageBoxW(nullptr, message, L"Game Crash", 0x12014) != IDYES)
    // {
    //     return;
    // }
    const EXCEPTION_RECORD* pExceptionRecord = pExceptionPointers != nullptr ? pExceptionPointers->ExceptionRecord : nullptr;
    keen::debug::nativePrintArgs("[crash] Game Crash: exception 0x%08x at 0x%08x.\n", pExceptionRecord != nullptr ? (unsigned int)pExceptionRecord->ExceptionCode : 0u,
                                 pExceptionRecord != nullptr ? (unsigned int)(uintptr_t)pExceptionRecord->ExceptionAddress : 0u);
    if (keen::generateDump(pExceptionPointers))
    {
        char dumpFileName[260];
        char logFileName[260];
        keen::convertUCS2ToUTF8(dumpFileName, sizeof(dumpFileName), (const uint16*)s_dumpFileName);
        keen::convertUCS2ToUTF8(logFileName, sizeof(logFileName), (const uint16*)s_logFileName);
        keen::debug::shutdownLogFile();
        keen::uploadToBacktrace(dumpFileName, logFileName);
        return;
    }
    // OpenRFG deviation: the message box is replaced by a log line.
    // Magic number: MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND.
    // MessageBoxW(nullptr, L"Unable to write diagnostic file.", L"The Game Crashed!", 0x12010);
    keen::debug::nativePrintArgs("[crash] The Game Crashed! Unable to write diagnostic file.\n");
}

// 0x00C10090
static LONG WINAPI handleUnhandledException(EXCEPTION_POINTERS* pExceptionPointers)
{
    handleFatalError(pExceptionPointers);
    return EXCEPTION_EXECUTE_HANDLER;
}

namespace keen
{
    namespace GameFramework
    {
        // 0x00C0EE90
        // Steam's language when Steam runs, else the user's locale ("en-US" style); English when it is not one of keen's.
        LanguageId getSystemLanguage(const GameFrameworkSystem* pGameFramework)
        {
            if (s_overrideSystemLanguage)
            {
                return s_systemLanguage;
            }
            if (pGameFramework->pSteamworksSystem != nullptr)
            {
                return getCurrentSteamworksLanguageId(pGameFramework->pSteamworksSystem);
            }
            char languageName[8] = {};
            char countryName[8] = {};
            if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, languageName, 5) == 0 ||
                GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, countryName, 5) == 0)
            {
                return LanguageId_En_En;
            }
            char localeName[12];
            formatString(localeName, 10, "%s-%s", languageName, countryName);
            const LanguageId language = parseLanguageIdFromString(localeName);
            if (language == LanguageId_Count)
            {
                return LanguageId_En_En;
            }
            return language;
        }

        // 0x00C0EF80
        RegionId getSystemRegion(const GameFrameworkSystem*)
        {
            return RegionId_Unknown;
        }

        // 0x00C0EF90
        void parsePlatformArguments(GameFrameworkParameters* pParameters, int argc, char** argv)
        {
            for (int i = 0; i < argc; ++i)
            {
                if (isStringEqual(argv[i], "-autopos"))
                {
                    pParameters->autoPosWindows = true;
                    return;
                }
            }
        }

        // 0x00C0EFD0
        void processFatalError(GameFrameworkSystem*, FatalErrorTypeId)
        {
            ExitProcess(1);
        }

        // 0x00C0EFE0
        // Denormals flush to zero (the precision stays the CRT's); the window class "localWindowClass"; the log file
        // <exe dir>/<project>.log; the window: 1280x720 client area unless the parameters give a size, a caption,
        // system menu and minimize box (plus a sizing frame and maximize box when resizeable), or a popup over the whole
        // screen when borderless, or a child of the parent window when there is one.
        // Original behaviour kept: when the window class cannot be registered or the window cannot be created it
        // returns without a window (initializeDisplay then fails).
        void initializeBase(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters)
        {
            _controlfp(_DN_FLUSH, _MCW_DN);
            pGameFramework->hwnd = nullptr;
            pGameFramework->quit = false;
            pGameFramework->showBreakHandlerMessage = true;
            s_pCustomCrashDumpCallback = parameters.pCustomCrashDumpCallback;
            s_windowAspectRatioConstraintData = parameters.aspectRatioConstraintData;
            pGameFramework->hideMouseCursorInFullscreen = parameters.hideMouseCursorInFullscreen;
            pGameFramework->quitWithEscape = parameters.quitWithEscape;
            pGameFramework->resizeableWindow = parameters.resizeableWindow;
            pGameFramework->triggerAltEnter = 0;
            if (!parameters.headLess)
            {
                WNDCLASSW windowClass = {};
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.lpfnWndProc = wndProc;
                windowClass.cbWndExtra = sizeof(GameFrameworkSystem*);
                windowClass.lpszClassName = L"localWindowClass";
                // Magic number: COLOR_INACTIVECAPTIONTEXT + 1 as the background brush.
                windowClass.hbrBackground = (HBRUSH)9;
                // Magic number: IDC_ARROW.
                windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(0x7F00));
                windowClass.hIcon = LoadIconW(parameters.isStandardIcon ? nullptr : windowClass.hInstance, parameters.windowIcon);
                if (RegisterClassW(&windowClass) == 0)
                {
                    return;
                }
            }
            WCHAR exePathUtf16[260];
            GetModuleFileNameW(nullptr, exePathUtf16, 0x104);
            char exePath[520];
            convertUTF16ToUTF8(exePath, sizeof(exePath), (const uint16*)exePathUtf16);
            FilePath logPath(exePath, false);
            logPath.setFileName(parameters.pProjectName);
            logPath.setExtension(".log");
            convertUTF8ToUTF16((uint16*)s_logFileName, sizeof(s_logFileName), logPath.getCompletePath());
            debug::initializeLogFile(logPath.getCompletePath());
            debug::nativePrintArgs("Logfile created for %s, %s.\n", getBuildVersionString(), getBuildProjectName());
            if (!parameters.headLess)
            {
                DWORD style;
                if (!parameters.borderLessWindow)
                {
                    // Magic numbers: WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, plus WS_THICKFRAME | WS_MAXIMIZEBOX.
                    style = (pGameFramework->resizeableWindow ? 0x50000u : 0u) + 0xCA0000u;
                }
                else
                {
                    // Magic number: WS_POPUP | WS_CLIPCHILDREN.
                    style = 0x82000000u;
                }
                RECT rect;
                if (parameters.screenWidth == 0 || parameters.screenHeight == 0)
                {
                    // Magic numbers: the default 1280x720 client area.
                    SetRect(&rect, parameters.windowPosX, parameters.windowPosY, parameters.windowPosX + 0x500, parameters.windowPosY + 0x2D0);
                }
                else
                {
                    SetRect(&rect, parameters.windowPosX, parameters.windowPosY, parameters.windowPosX + parameters.screenWidth, parameters.screenHeight + parameters.windowPosY);
                }
                char title[520];
                copyString(title, 0x104, parameters.pProjectName);
                WCHAR titleUtf16[520];
                convertUTF8ToUTF16((uint16*)titleUtf16, 0x410, title);
                HWND hParent = GetDesktopWindow();
                if (parameters.pParentWindowHandle != nullptr)
                {
                    style = WS_CHILD;
                    hParent = (HWND)parameters.pParentWindowHandle;
                }
                AdjustWindowRect(&rect, style, FALSE);
                if (parameters.borderLessWindow)
                {
                    rect.left = 0;
                    rect.top = 0;
                    rect.right = GetSystemMetrics(SM_CXSCREEN);
                    rect.bottom = GetSystemMetrics(SM_CYSCREEN);
                }
                HWND hWnd = CreateWindowExW(0, L"localWindowClass", titleUtf16, style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                                            hParent, nullptr, GetModuleHandleW(nullptr), nullptr);
                pGameFramework->hwnd = hWnd;
                if (hWnd == nullptr)
                {
                    return;
                }
                SetWindowLongW(hWnd, 0, (LONG)pGameFramework);
            }
            s_languageFamily = getLanguageFamily(getSystemLanguage(pGameFramework));
        }

        // 0x00C0F300
        void shutdownBase(GameFrameworkSystem*)
        {
        }

        // 0x00C0F310
        // keen graphics on the framework's window: a 64 KB immediate geometry buffer (unless the parameters give one),
        // 40 MB and 512 instances of skinning buffer, the shader objects in memory block 11, the default swap chain
        // with vsync. Then the 1 ms timer resolution.
        GraphicsSystem* initializeDisplay(GameFrameworkSystem* pGameFramework, const GameFrameworkParameters& parameters)
        {
            if (parameters.headLess)
            {
                return nullptr;
            }
            ShowWindow(pGameFramework->hwnd, SW_SHOW);
            GraphicsSystemParameters graphicsParameters;
            graphicsParameters.pWindowHandle = pGameFramework->hwnd;
            graphicsParameters.pWindowTitle = "keen graphics window";
            graphicsParameters.hInstance = nullptr;
            graphicsParameters.nShowCmd = SW_SHOW;
            // Magic numbers: 64 KB immediate geometry, 512 skinned instances in 40 MB.
            graphicsParameters.immediateGeometryBufferSize = 0x10000u;
            graphicsParameters.supportScaling = true;
            graphicsParameters.createDefaultSwapChain = true;
            graphicsParameters.defaultDepthBufferFormat = PixelFormat_Default_Depth;
            graphicsParameters.defaultSwapChainPresentationInterval = 1;
            graphicsParameters.pTaskSystem = nullptr;
            graphicsParameters.skinningBufferMaxInstanceCount = 0x200u;
            graphicsParameters.skinningBufferSizeInBytes = 0x2800000u;
            if (parameters.immediateGeometryBufferSize != 0xFFFFFFFFu)
            {
                graphicsParameters.immediateGeometryBufferSize = parameters.immediateGeometryBufferSize;
            }
            graphicsParameters.pShaderFileSystem = pGameFramework->pFileSystem;
            // Magic number: block 11 holds the shader objects.
            graphicsParameters.shaderObjectMemory = memory::lockMemoryBlock(pGameFramework->pMemorySystem, 11u);
            graphicsParameters.pTaskSystem = pGameFramework->pTaskSystem;
            GraphicsSystem* pGraphicsSystem = graphics::createGraphicsSystem(pGameFramework->pDefaultAllocator, graphicsParameters);
            if (pGraphicsSystem == nullptr)
            {
                debug::nativePrintArgs("Could not create graphics system!\n");
                return nullptr;
            }
            pGameFramework->wasInFullscreen = graphics::isInFullScreenMode(pGraphicsSystem, nullptr);
            timeBeginPeriod(1);
            return pGraphicsSystem;
        }

        // 0x00C0F420
        void shutdownDisplay(GameFrameworkSystem* pGameFramework)
        {
            timeEndPeriod(1);
            graphics::destroyGraphicsSystem(pGameFramework->pGraphicsSystem);
            pGameFramework->pGraphicsSystem = nullptr;
            memory::unlockMemoryBlock(pGameFramework->pMemorySystem, 11u);
            DestroyWindow(pGameFramework->hwnd);
            pGameFramework->hwnd = nullptr;
        }

        // 0x00C0F470
        // A pending Alt+Enter toggles the window mode; the cursor hides on entering fullscreen and shows on leaving it
        // (when the parameters ask for it); then the window messages.
        void updateBase(GameFrameworkSystem* pGameFramework, float)
        {
            if (pGameFramework->triggerAltEnter > 0)
            {
                pGameFramework->triggerAltEnter = 0;
                graphics::toggleWindowMode(pGameFramework->pGraphicsSystem, nullptr);
            }
            const bool isInFullscreen = graphics::isInFullScreenMode(pGameFramework->pGraphicsSystem, nullptr);
            if (pGameFramework->hideMouseCursorInFullscreen)
            {
                if (!pGameFramework->wasInFullscreen)
                {
                    if (isInFullscreen)
                    {
                        input::setMouseCursorVisible(pGameFramework->pInputSystem, false, true);
                    }
                }
                else if (!isInFullscreen)
                {
                    input::setMouseCursorVisible(pGameFramework->pInputSystem, true, true);
                }
            }
            pGameFramework->wasInFullscreen = isInFullscreen;
            handleMessages(pGameFramework);
        }

        namespace Internal
        {
            // 0x00C0F500
            void renderPlatform(GameFrameworkSystem*, GraphicsCommandWriter*)
            {
            }

            // 0x00C0F510
            void lateInitializePlatform(GameFrameworkSystem*)
            {
            }

            // 0x00C0F530
            bool getLocalAccountName(char* pName, uint32 capacity)
            {
                size_t length;
                return getenv_s(&length, pName, capacity, "USERNAME") == 0 && length != 0;
            }
        }

        // 0x00C0F520
        bool wantsToQuit(const GameFrameworkSystem* pGameFramework)
        {
            return pGameFramework->quit;
        }

        // 0x00C0F560
        void quitGame(GameFrameworkSystem* pFramework)
        {
            pFramework->quit = true;
        }

        // 0x00C0F570
        void minimizeGameWindow(GameFrameworkSystem* pFramework)
        {
            ShowWindow(pFramework->hwnd, SW_MINIMIZE);
        }

        // 0x00C0FC30
        // The client area's size (0 when it is empty or GetClientRect fails) and the window.
        bool getGameFrameworkWindowInfo(GameFrameworkWindowInfo* pWindowInfo, const GameFrameworkSystem* pGameFramework)
        {
            pWindowInfo->pWindowHandle = pGameFramework->hwnd;
            RECT rect;
            if (GetClientRect(pGameFramework->hwnd, &rect) == TRUE && rect.top < rect.bottom)
            {
                const int width = rect.right - rect.left;
                const int height = rect.bottom - rect.top;
                pWindowInfo->width = width > 0 ? (uint32)width : 0u;
                pWindowInfo->height = height > 0 ? (uint32)height : 0u;
                return true;
            }
            pWindowInfo->width = 0;
            pWindowInfo->height = 0;
            return false;
        }
    }
}

// 0x00C10E80
// The process entry: the instance, the Steam boot invite of the command line (+connect_lobby LOBBYID becomes the game
// link's invitation data), the crash handler, then the framework's main with the CRT's arguments.
// Original behaviour kept: the lobby id is read in place (argv's entry is left past the digits).
// 0x00C10D00 (:1014)
// Registers for Steam's GameLobbyJoinRequested_t (callback 0x14D), with SW_CCSys_InitCallbackFunc, before Steam starts.
GameLinkInvitationHandler::GameLinkInvitationHandler()
    : m_callbackLobbyJoinRequested(this, &GameLinkInvitationHandler::handleLobbyJoinRequest)
{
}

// 0x00C100B0 (:1020)
// A lobby join request from the Steam overlay becomes the game link's invitation data (lobby and friend).
void GameLinkInvitationHandler::handleLobbyJoinRequest(GameLobbyJoinRequested_t* pCallback)
{
    if (pCallback != nullptr && pCallback->m_steamIDLobby.IsValid())
    {
        kaiko::GameLinkInvitationData invitationData;
        invitationData.lobbyId = pCallback->m_steamIDLobby;
        invitationData.friendId = pCallback->m_steamIDFriend;
        kaiko::gamelink::setInvitationData(&invitationData);
    }
}

// 0x00C10130
// The callback's destructor unregisters it (SW_CCSys_RemoveCallbackFunc) when it is registered.
GameLinkInvitationHandler::~GameLinkInvitationHandler()
{
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int)
{
    keen::g_hInstance = hInstance;
    keen::g_prevInstance = hPrevInstance;
    // On this stack frame for the whole run (rfg.exe inlines the constructor and the destructor here). It registers
    // with SW_API.DLL whether or not Steam starts later (OpenRFG's --no-steam), as rfg.exe always does.
    GameLinkInvitationHandler invitationHandler;
    for (int i = 0; i < __argc; ++i)
    {
        if (keen::isStringEqual(__argv[i], "+connect_lobby") && i + 1 < __argc)
        {
            uint64 lobbyId;
            if (keen::readSignLessUint64FromDecimalString(&lobbyId, (const char**)&__argv[i + 1]) == keen::StringReadResult_Ok)
            {
                kaiko::GameLinkInvitationData invitationData;
                invitationData.lobbyId = CSteamID(lobbyId);
                kaiko::gamelink::setInvitationData(&invitationData);
            }
        }
    }
    SetUnhandledExceptionFilter(handleUnhandledException);
    return keen::GameFramework::main(__argc, __argv);
}
