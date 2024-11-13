#define WIN32_LEAN_AND_MEAN

// Include Windows headers first
#include <windows.h>

// Include third-party and standard library headers
#include <dpp/dpp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <sstream>
#include <cctype> // For tolower
#include <algorithm> // For std::transform
#include <fstream>
#include "json.hpp" // Ensure this path is correct
using json = nlohmann::json;

// Global variables for GUI inputs
std::string botToken = "";
int delay_ms = 3000;
std::vector<int> cjbKeys = { '1', '2', '3', '4', '5', '6', '7', '8' };
std::vector<int> ascKeys = { '1' };
std::vector<int> shaKeys = { '1', '2', '3' };

// Window handles
HWND CJB_WindowHandle = nullptr;
HWND ASC_WindowHandle = nullptr;
HWND SHA_WindowHandle = nullptr;

// Mutex for thread safety
std::mutex mtx;

// Enumeration for handle types
enum class HandleType { CJB, ASC, SHA, None };

// Current handle type being set
HandleType currentHandleType = HandleType::None;

// Global GUI window handle
HWND mainGuiWindow = nullptr;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateGUI();
void sendKeyPress(HWND hwnd, int key);
void sendKeySequence(HWND hwnd, const std::vector<int>& keys);
std::vector<int> ParseKeySequence(const std::string& keySequence);
void SaveSettings(const std::string& filename);
void LoadSettings(const std::string& filename);

// Controls' IDs
#define ID_TOKEN_INPUT      101
#define ID_DELAY_INPUT      102
#define ID_SAVE_BUTTON      103
#define ID_SET_CJB_BUTTON   104
#define ID_SET_ASC_BUTTON   105
#define ID_SET_SHA_BUTTON   106
#define ID_CJB_LABEL        107
#define ID_ASC_LABEL        108
#define ID_SHA_LABEL        109
#define ID_CJB_KEYS_INPUT   110
#define ID_ASC_KEYS_INPUT   111
#define ID_SHA_KEYS_INPUT   112
#define ID_CJB_KEYS_LABEL   113
#define ID_ASC_KEYS_LABEL   114
#define ID_SHA_KEYS_LABEL   115

// Custom message for assigning window handles
#define WM_ASSIGN_WINDOW    (WM_USER + 1)

// Hook handle
HHOOK hMouseHook = NULL;

// Forward declaration of the hook callback
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

// Function to set the mouse hook
void SetMouseHook()
{
    if (hMouseHook == NULL)
    {
        hMouseHook = SetWindowsHookExA(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleA(NULL), 0);
        if (hMouseHook == NULL)
        {
            MessageBoxA(NULL, "Failed to install mouse hook!", "Error", MB_ICONERROR);
        }
    }
}

// Function to remove the mouse hook
void RemoveMouseHook()
{
    if (hMouseHook != NULL)
    {
        UnhookWindowsHookEx(hMouseHook);
        hMouseHook = NULL;
    }
}

// Helper function to convert a string to lowercase
std::string ToLowerCase(const std::string& str)
{
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return lowerStr;
}

// Function to parse comma-separated key strings into a vector of virtual key codes
std::vector<int> ParseKeySequence(const std::string& keySequence)
{
    std::vector<int> keys;
    std::string token;
    std::istringstream tokenStream(keySequence);

    while (std::getline(tokenStream, token, ',')) {
        // Trim whitespace
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());

        if (token.empty()) continue;

        // Convert to uppercase for consistency
        std::transform(token.begin(), token.end(), token.begin(), ::toupper);

        // Handle special keys if needed (e.g., ENTER, SPACE)
        if (token == "ENTER") {
            keys.push_back(VK_RETURN);
        }
        else if (token == "SPACE") {
            keys.push_back(VK_SPACE);
        }
        else if (token.size() == 1) {
            // Assume single character keys
            char ch = token[0];
            keys.push_back(static_cast<int>(ch));
        }
        else {
            // Invalid key format
            std::cerr << "Invalid key: " << token << ". Skipping." << std::endl;
        }
    }

    return keys;
}

// Hook callback implementation
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_LBUTTONDOWN)
        {
            MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
            if (pMouseStruct != NULL)
            {
                POINT pt = pMouseStruct->pt;
                HWND clickedWnd = WindowFromPoint(pt);
                if (clickedWnd != NULL && clickedWnd != mainGuiWindow)
                {
                    char windowTitle[256];
                    GetWindowTextA(clickedWnd, windowTitle, sizeof(windowTitle));

                    // Convert window title to lowercase for case-insensitive comparison
                    std::string windowTitleStr = ToLowerCase(std::string(windowTitle));
                    std::string targetTitle = ToLowerCase("KalOnline");

                    if (windowTitleStr == targetTitle)
                    {
                        // Send custom message to main GUI window
                        PostMessageA(mainGuiWindow, WM_ASSIGN_WINDOW, (WPARAM)currentHandleType, (LPARAM)clickedWnd);

                        std::cout << "Assigned Window: " << windowTitle << " (HWND: " << clickedWnd << ")" << std::endl;

                        // Reset the handle type
                        currentHandleType = HandleType::None;

                        // Remove the hook
                        RemoveMouseHook();

                        // Prevent further processing of this click
                        return 1;
                    }
                    else
                    {
                        // Inform the user that it's not the correct window
                        std::cout << "Selected window \"" << windowTitle << "\" is not \"KalOnline\". Please select the correct window." << std::endl;

                        // Audible beep to indicate incorrect selection
                        Beep(750, 300);

                        // Keep the hook active for another selection
                        // Optionally, limit the number of attempts or provide a cancellation mechanism
                    }
                }
            }
        }
    }

    // Pass the event to the next hook
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Entry point for the GUI thread
void CreateGUI() {
    // Window class structure
    WNDCLASSA wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "KDB";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Standard window background

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Error", MB_ICONERROR);
        return;
    }

    // Create the window
    HWND hwnd = CreateWindowExA(
        0,
        "KDB",
        "Ricks - KalOnline Discord Buffbot",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 500, // Increased default size
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );

    if (hwnd == NULL) {
        MessageBoxA(NULL, "Failed to create window.", "Error", MB_ICONERROR);
        return;
    }

    // Assign the global GUI window handle
    mainGuiWindow = hwnd;

    ShowWindow(hwnd, SW_SHOW);

    // Message loop
    MSG msg = { };
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// Window procedure to handle GUI events
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Static variables to store control handles
    static HWND hTokenInput, hDelayInput, hSaveButton;
    static HWND hSetCJBButton, hSetASCButton, hSetSHAButton;
    static HWND hCJBLabel, hASCLabel, hSHALabel;
    static HWND hCJBKeysInput, hASCKeysInput, hSHAKeysInput;
    static HWND hCJBKeysLabel, hASCKeysLabel, hSHAKeysLabel;

    switch (uMsg) {
    case WM_CREATE:
        // Use a consistent font for all controls
    {
        NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        HFONT hFont = CreateFontIndirect(&ncm.lfMessageFont);
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // Create input fields and buttons

    // Bot Token Label
    CreateWindowA(
        "STATIC",
        "Bot Token:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 20, 120, 25,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    // Bot Token Input
    hTokenInput = CreateWindowA(
        "EDIT",
        "",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        150, 20, 500, 25,
        hwnd,
        (HMENU)ID_TOKEN_INPUT,
        NULL,
        NULL
    );

    // Delay Label
    CreateWindowA(
        "STATIC",
        "Delay (ms):",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 60, 120, 25,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    // Delay Input
    hDelayInput = CreateWindowA(
        "EDIT",
        "3000",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        150, 60, 200, 25,
        hwnd,
        (HMENU)ID_DELAY_INPUT,
        NULL,
        NULL
    );

    // Save Settings Button
    hSaveButton = CreateWindowA(
        "BUTTON",
        "Save Settings",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 100, 150, 35,
        hwnd,
        (HMENU)ID_SAVE_BUTTON,
        NULL,
        NULL
    );

    // Assign Window Handles Buttons
    hSetCJBButton = CreateWindowA(
        "BUTTON",
        "Set CJB Window",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 150, 150, 35,
        hwnd,
        (HMENU)ID_SET_CJB_BUTTON,
        NULL,
        NULL
    );

    hSetASCButton = CreateWindowA(
        "BUTTON",
        "Set ASC Window",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 200, 150, 35,
        hwnd,
        (HMENU)ID_SET_ASC_BUTTON,
        NULL,
        NULL
    );

    hSetSHAButton = CreateWindowA(
        "BUTTON",
        "Set SHA Window",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 250, 150, 35,
        hwnd,
        (HMENU)ID_SET_SHA_BUTTON,
        NULL,
        NULL
    );

    // Labels to display assigned window titles

    // CJB Window Label
    CreateWindowA(
        "STATIC",
        "CJB Window:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        200, 150, 120, 35,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hCJBLabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        330, 150, 320, 35,
        hwnd,
        (HMENU)ID_CJB_LABEL,
        NULL,
        NULL
    );

    // ASC Window Label
    CreateWindowA(
        "STATIC",
        "ASC Window:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        200, 200, 120, 35,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hASCLabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        330, 200, 320, 35,
        hwnd,
        (HMENU)ID_ASC_LABEL,
        NULL,
        NULL
    );

    // SHA Window Label
    CreateWindowA(
        "STATIC",
        "SHA Window:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        200, 250, 120, 35,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hSHALabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        330, 250, 320, 35,
        hwnd,
        (HMENU)ID_SHA_LABEL,
        NULL,
        NULL
    );

    // CJB Keys
    CreateWindowA(
        "STATIC",
        "CJB Keys (e.g., A,B,C):",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 320, 150, 25,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hCJBKeysInput = CreateWindowA(
        "EDIT",
        "1,2,3,4,5,6,7,8", // Default keys
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        180, 320, 400, 25,
        hwnd,
        (HMENU)ID_CJB_KEYS_INPUT,
        NULL,
        NULL
    );

    hCJBKeysLabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        600, 320, 80, 25,
        hwnd,
        (HMENU)ID_CJB_KEYS_LABEL,
        NULL,
        NULL
    );

    // ASC Keys
    CreateWindowA(
        "STATIC",
        "ASC Keys (e.g., A,B,C):",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 360, 150, 25,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hASCKeysInput = CreateWindowA(
        "EDIT",
        "1", // Default keys
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        180, 360, 400, 25,
        hwnd,
        (HMENU)ID_ASC_KEYS_INPUT,
        NULL,
        NULL
    );

    hASCKeysLabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        600, 360, 80, 25,
        hwnd,
        (HMENU)ID_ASC_KEYS_LABEL,
        NULL,
        NULL
    );

    // SHA Keys
    CreateWindowA(
        "STATIC",
        "SHA Keys (e.g., A,B,C):",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 400, 150, 25,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    hSHAKeysInput = CreateWindowA(
        "EDIT",
        "1,2,3", // Default keys
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        180, 400, 400, 25,
        hwnd,
        (HMENU)ID_SHA_KEYS_INPUT,
        NULL,
        NULL
    );

    hSHAKeysLabel = CreateWindowA(
        "STATIC",
        "Not Set",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        600, 400, 80, 25,
        hwnd,
        (HMENU)ID_SHA_KEYS_LABEL,
        NULL,
        NULL
    );

    break;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;

        // Set background mode to transparent
        SetBkMode(hdcStatic, TRANSPARENT);

        // Set text color to black
        SetTextColor(hdcStatic, RGB(0, 0, 0));

        // Return a handle to the white brush for background
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }

    case WM_SIZE:
    {
        // Get the new width and height of the client area
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Define padding and control dimensions
        int padding = 20;
        int labelWidth = 150;
        int inputWidth = width - 3 * padding - labelWidth - 80; // Adjust as needed
        int buttonWidth = 150;
        int buttonHeight = 35;

        // Set minimum window size to prevent overlapping controls
        if (width < 700) width = 700;
        if (height < 500) height = 500;

        // Resize Bot Token Input
        MoveWindow(hTokenInput, padding + labelWidth + 10, 20, width - 3 * padding - labelWidth - 10, 25, TRUE);

        // Resize Delay Input
        MoveWindow(hDelayInput, padding + labelWidth + 10, 60, 200, 25, TRUE);

        // Resize Save Settings Button
        MoveWindow(hSaveButton, padding, 100, buttonWidth, buttonHeight, TRUE);

        // Resize Set Window Buttons
        MoveWindow(hSetCJBButton, padding, 150, buttonWidth, buttonHeight, TRUE);
        MoveWindow(hSetASCButton, padding, 200, buttonWidth, buttonHeight, TRUE);
        MoveWindow(hSetSHAButton, padding, 250, buttonWidth, buttonHeight, TRUE);

        // Resize Labels for Assigned Windows
        MoveWindow(hCJBLabel, 330, 150, width - 3 * padding - 330 - 80, 35, TRUE);
        MoveWindow(hASCLabel, 330, 200, width - 3 * padding - 330 - 80, 35, TRUE);
        MoveWindow(hSHALabel, 330, 250, width - 3 * padding - 330 - 80, 35, TRUE);

        // Resize CJB Keys Input and Label
        MoveWindow(hCJBKeysInput, 180, 320, width - 3 * padding - 180 - 80, 25, TRUE);
        MoveWindow(hCJBKeysLabel, width - padding - 80, 320, 80, 25, TRUE);

        // Resize ASC Keys Input and Label
        MoveWindow(hASCKeysInput, 180, 360, width - 3 * padding - 180 - 80, 25, TRUE);
        MoveWindow(hASCKeysLabel, width - padding - 80, 360, 80, 25, TRUE);

        // Resize SHA Keys Input and Label
        MoveWindow(hSHAKeysInput, 180, 400, width - 3 * padding - 180 - 80, 25, TRUE);
        MoveWindow(hSHAKeysLabel, width - padding - 80, 400, 80, 25, TRUE);

        // Invalidate the entire window to ensure proper repainting
        InvalidateRect(hwnd, NULL, TRUE);

        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_SAVE_BUTTON:
        {
            // Get Bot Token
            char tokenBuffer[512];
            GetWindowTextA(GetDlgItem(hwnd, ID_TOKEN_INPUT), tokenBuffer, sizeof(tokenBuffer));
            {
                std::lock_guard<std::mutex> lock(mtx);
                botToken = std::string(tokenBuffer);
            }

            // Get Delay
            char delayBuffer[16];
            GetWindowTextA(GetDlgItem(hwnd, ID_DELAY_INPUT), delayBuffer, sizeof(delayBuffer));
            {
                std::lock_guard<std::mutex> lock(mtx);
                try {
                    delay_ms = std::stoi(delayBuffer);
                }
                catch (const std::exception& e) {
                    MessageBoxA(hwnd, "Invalid delay value. Please enter a number.", "Error", MB_ICONERROR);
                    delay_ms = 3000; // Reset to default
                }
            }

            // Get CJB Keys
            char cjbKeysBuffer[256];
            GetWindowTextA(GetDlgItem(hwnd, ID_CJB_KEYS_INPUT), cjbKeysBuffer, sizeof(cjbKeysBuffer));
            {
                std::lock_guard<std::mutex> lock(mtx);
                cjbKeys = ParseKeySequence(std::string(cjbKeysBuffer));
                if (!cjbKeys.empty()) {
                    SetWindowTextA(GetDlgItem(hwnd, ID_CJB_KEYS_LABEL), "Configured");
                }
                else {
                    SetWindowTextA(GetDlgItem(hwnd, ID_CJB_KEYS_LABEL), "Invalid");
                }
            }

            // Get ASC Keys
            char ascKeysBuffer[256];
            GetWindowTextA(GetDlgItem(hwnd, ID_ASC_KEYS_INPUT), ascKeysBuffer, sizeof(ascKeysBuffer));
            {
                std::lock_guard<std::mutex> lock(mtx);
                ascKeys = ParseKeySequence(std::string(ascKeysBuffer));
                if (!ascKeys.empty()) {
                    SetWindowTextA(GetDlgItem(hwnd, ID_ASC_KEYS_LABEL), "Configured");
                }
                else {
                    SetWindowTextA(GetDlgItem(hwnd, ID_ASC_KEYS_LABEL), "Invalid");
                }
            }

            // Get SHA Keys
            char shaKeysBuffer[256];
            GetWindowTextA(GetDlgItem(hwnd, ID_SHA_KEYS_INPUT), shaKeysBuffer, sizeof(shaKeysBuffer));
            {
                std::lock_guard<std::mutex> lock(mtx);
                shaKeys = ParseKeySequence(std::string(shaKeysBuffer));
                if (!shaKeys.empty()) {
                    SetWindowTextA(GetDlgItem(hwnd, ID_SHA_KEYS_LABEL), "Configured");
                }
                else {
                    SetWindowTextA(GetDlgItem(hwnd, ID_SHA_KEYS_LABEL), "Invalid");
                }
            }

            // Save settings to file
            SaveSettings("config.json");

            MessageBoxA(hwnd, "Settings saved successfully.", "Info", MB_OK);
            break;
        }

        case ID_SET_CJB_BUTTON:
        {
            // Set the current handle type
            {
                std::lock_guard<std::mutex> lock(mtx);
                currentHandleType = HandleType::CJB;
            }

            // Hide the GUI window to allow window selection
            ShowWindow(hwnd, SW_HIDE);

            // Set the mouse hook
            SetMouseHook();

            // Inform the user via console
            std::cout << "Please click on the \"KalOnline\" window to assign as CJB Window." << std::endl;
            break;
        }

        case ID_SET_ASC_BUTTON:
        {
            // Set the current handle type
            {
                std::lock_guard<std::mutex> lock(mtx);
                currentHandleType = HandleType::ASC;
            }

            // Hide the GUI window to allow window selection
            ShowWindow(hwnd, SW_HIDE);

            // Set the mouse hook
            SetMouseHook();

            // Inform the user via console
            std::cout << "Please click on the \"KalOnline\" window to assign as ASC Window." << std::endl;
            break;
        }

        case ID_SET_SHA_BUTTON:
        {
            // Set the current handle type
            {
                std::lock_guard<std::mutex> lock(mtx);
                currentHandleType = HandleType::SHA;
            }

            // Hide the GUI window to allow window selection
            ShowWindow(hwnd, SW_HIDE);

            // Set the mouse hook
            SetMouseHook();

            // Inform the user via console
            std::cout << "Please click on the \"KalOnline\" window to assign as SHA Window." << std::endl;
            break;
        }

        default:
            break;
        }
        break;

    case WM_ASSIGN_WINDOW:
    {
        HandleType type = static_cast<HandleType>(wParam);
        HWND clickedWnd = (HWND)lParam;

        char windowTitle[256];
        GetWindowTextA(clickedWnd, windowTitle, sizeof(windowTitle));

        std::lock_guard<std::mutex> lock(mtx);

        std::string displayText = std::string(windowTitle) + " (HWND: " + std::to_string(reinterpret_cast<uintptr_t>(clickedWnd)) + ")";

        switch (type)
        {
        case HandleType::CJB:
            CJB_WindowHandle = clickedWnd;
            SetWindowTextA(GetDlgItem(hwnd, ID_CJB_LABEL), displayText.c_str());
            std::cout << "CJB_WindowHandle set to: " << clickedWnd << " (" << windowTitle << ")" << std::endl;
            break;
        case HandleType::ASC:
            ASC_WindowHandle = clickedWnd;
            SetWindowTextA(GetDlgItem(hwnd, ID_ASC_LABEL), displayText.c_str());
            std::cout << "ASC_WindowHandle set to: " << clickedWnd << " (" << windowTitle << ")" << std::endl;
            break;
        case HandleType::SHA:
            SHA_WindowHandle = clickedWnd;
            SetWindowTextA(GetDlgItem(hwnd, ID_SHA_LABEL), displayText.c_str());
            std::cout << "SHA_WindowHandle set to: " << clickedWnd << " (" << windowTitle << ")" << std::endl;
            break;
        default:
            break;
        }

        // Show the GUI window again
        ShowWindow(hwnd, SW_SHOW);

        break;
    }

    case WM_DESTROY:
        // Ensure the hook is removed if the window is destroyed
        RemoveMouseHook();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Function to send a single key press to a window
void sendKeyPress(HWND hwnd, int key) {
    std::lock_guard<std::mutex> lock(mtx);
    if (hwnd) {
        // Prepare lParam for keydown and keyup
        LPARAM lParamDown = 1 | (GetTickCount() << 16);
        LPARAM lParamUp = (1 << 30) | (1 << 31) | (GetTickCount() << 16);

        SendMessageA(hwnd, WM_KEYDOWN, key, lParamDown);
        SendMessageA(hwnd, WM_CHAR, key, lParamDown);
        SendMessageA(hwnd, WM_KEYUP, key, lParamUp);

        std::cout << "Sent keypress '" << static_cast<char>(key) << "' to window handle: " << hwnd << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    else {
        std::cout << "Window handle is not initialized." << std::endl;
    }
}

// Function to send a sequence of key presses to a window
void sendKeySequence(HWND hwnd, const std::vector<int>& keys) {
    for (int key : keys) {
        sendKeyPress(hwnd, key);
    }
}

// Function to save settings to a JSON file
void SaveSettings(const std::string& filename)
{
    json j;
    std::lock_guard<std::mutex> lock(mtx);
    j["bot_token"] = botToken;
    j["delay_ms"] = delay_ms;

    // Convert key sequences to strings
    auto KeysToString = [](const std::vector<int>& keys) -> std::string {
        std::string s;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == VK_RETURN) {
                s += "ENTER";
            }
            else if (keys[i] == VK_SPACE) {
                s += "SPACE";
            }
            else {
                s += static_cast<char>(keys[i]);
            }
            if (i != keys.size() - 1) s += ",";
        }
        return s;
        };

    j["cjb_keys"] = KeysToString(cjbKeys);
    j["asc_keys"] = KeysToString(ascKeys);
    j["sha_keys"] = KeysToString(shaKeys);

    // Save window handles as hexadecimal strings
    auto HWNDToHex = [](HWND hwnd) -> std::string {
        std::ostringstream oss;
        oss << std::hex << reinterpret_cast<uintptr_t>(hwnd);
        return oss.str();
        };

    j["cjb_window_handle"] = HWNDToHex(CJB_WindowHandle);
    j["asc_window_handle"] = HWNDToHex(ASC_WindowHandle);
    j["sha_window_handle"] = HWNDToHex(SHA_WindowHandle);

    // Write to file
    std::ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(4); // Pretty-print with 4-space indentation
        file.close();
        std::cout << "Settings saved to " << filename << std::endl;
    }
    else {
        std::cerr << "Failed to open " << filename << " for writing." << std::endl;
    }
}

// Function to load settings from a JSON file
void LoadSettings(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Settings file " << filename << " not found. Using default settings." << std::endl;
        return;
    }

    json j;
    file >> j;
    file.close();

    std::lock_guard<std::mutex> lock(mtx);
    botToken = j.value("bot_token", "");
    delay_ms = j.value("delay_ms", 3000);

    // Parse key sequences
    auto StringToKeys = [](const std::string& s) -> std::vector<int> {
        std::vector<int> keys;
        std::istringstream tokenStream(s);
        std::string token;
        while (std::getline(tokenStream, token, ',')) {
            if (token.empty()) continue;
            std::transform(token.begin(), token.end(), token.begin(), ::toupper);
            if (token == "ENTER") {
                keys.push_back(VK_RETURN);
            }
            else if (token == "SPACE") {
                keys.push_back(VK_SPACE);
            }
            else if (token.size() == 1) {
                char ch = token[0];
                keys.push_back(static_cast<int>(ch));
            }
            else {
                std::cerr << "Invalid key in settings: " << token << ". Skipping." << std::endl;
            }
        }
        return keys;
        };

    cjbKeys = StringToKeys(j.value("cjb_keys", ""));
    ascKeys = StringToKeys(j.value("asc_keys", ""));
    shaKeys = StringToKeys(j.value("sha_keys", ""));

    // Parse window handles
    auto HexToHWND = [](const std::string& s) -> HWND {
        uintptr_t addr = 0;
        std::stringstream ss;
        ss << std::hex << s;
        ss >> addr;
        return reinterpret_cast<HWND>(addr);
        };

    CJB_WindowHandle = HexToHWND(j.value("cjb_window_handle", "0"));
    ASC_WindowHandle = HexToHWND(j.value("asc_window_handle", "0"));
    SHA_WindowHandle = HexToHWND(j.value("sha_window_handle", "0"));

    std::cout << "Settings loaded from " << filename << std::endl;
}

// Function to send a sequence of key presses to a window asynchronously
void sendKeySequenceAsync(HWND hwnd, const std::vector<int>& keys) {
    std::thread([hwnd, keys]() {
        for (int key : keys) {
            sendKeyPress(hwnd, key);
        }
        }).detach(); // Detach the thread to allow independent execution
}

int main() {
    // Load settings from file
    LoadSettings("config.json");

    // Start the GUI in a separate thread
    std::thread guiThread(CreateGUI);

    // Wait until the user inputs the bot token
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!botToken.empty()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Copy the botToken under mutex
    std::string localBotToken;
    {
        std::lock_guard<std::mutex> lock(mtx);
        localBotToken = botToken;
    }

    // Create D++ bot instance with the user-provided token
    dpp::cluster bot(localBotToken, dpp::i_default_intents | dpp::i_message_content);

    // Event handler: Log events
    bot.on_log(dpp::utility::cout_logger());

    // Event handler: React to messages
    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        std::cout << "Received message: " << event.msg.content << std::endl;
        if (event.msg.content == "Buff") {
            bot.message_create(dpp::message(event.msg.channel_id, "Buffing in Progress"));

            // Buff sequence
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (CJB_WindowHandle) {
                    // Send key sequence asynchronously
                    sendKeySequenceAsync(CJB_WindowHandle, cjbKeys);
                }
                else {
                    std::cout << "CJB_WindowHandle is not initialized." << std::endl;
                }

                if (ASC_WindowHandle) {
                    // Send key sequence asynchronously
                    sendKeySequenceAsync(ASC_WindowHandle, ascKeys);
                }
                else {
                    std::cout << "ASC_WindowHandle is not initialized." << std::endl;
                }

                if (SHA_WindowHandle) {
                    // Send key sequence asynchronously
                    sendKeySequenceAsync(SHA_WindowHandle, shaKeys);
                }
                else {
                    std::cout << "SHA_WindowHandle is not initialized." << std::endl;
                }
            }
        }
        });

    // Start the bot and catch exceptions
    try {
        bot.start(dpp::st_wait);
    }
    catch (const dpp::invalid_token_exception& e) {
        std::cerr << "Invalid token exception: " << e.what() << std::endl;
        return 1;
    }
    catch (const dpp::exception& e) {
        std::cerr << "DPP exception: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        return 1;
    }

    // Join threads when bot stops
    if (guiThread.joinable()) {
        guiThread.join();
    }

    return 0;
}