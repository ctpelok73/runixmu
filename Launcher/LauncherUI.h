#pragma once
#include <windows.h>
#include <string>

// Global constants
#define LAUNCHER_WIDTH 960
#define LAUNCHER_HEIGHT 600
#define LAUNCHER_TITLE "RUNIXMU"

// New Color Palette (RunixMU Style)
const COLORREF COL_BG_DARK = RGB(5, 5, 15);
const COLORREF COL_BG_LIGHT = RGB(15, 15, 30);
const COLORREF COL_METAL_DARK = RGB(30, 30, 35);
const COLORREF COL_METAL_MID = RGB(60, 60, 70);
const COLORREF COL_METAL_LIGHT = RGB(120, 120, 130);
const COLORREF COL_NEON_BLUE = RGB(0, 120, 255);
const COLORREF COL_NEON_CYAN = RGB(0, 200, 255);
const COLORREF COL_NEON_GLOW = RGB(0, 50, 100);
const COLORREF COL_TEXT_NORMAL = RGB(180, 180, 200);
const COLORREF COL_TEXT_HIGHLIGHT = RGB(220, 220, 255);
const COLORREF COL_BUTTON_NORMAL = RGB(10, 30, 60);
const COLORREF COL_BUTTON_HOVER = RGB(20, 50, 90);

// UI States
enum class LauncherState {
    CheckingUpdates,
    Downloading,
    ReadyToPlay,
    Error
};

// UI Element Types
enum class ButtonStyle {
    Primary,    // Start Game (Big, Glowing)
    Secondary,  // Nav Buttons (Rectangular, Bordered)
    Icon        // Small Icon Buttons
};

// Drawing Functions
void DrawSpaceBackground(HDC hdc, RECT* rect);
void DrawMetallicFrame(HDC hdc, RECT* rect, int thickness);
void DrawGlowingGem(HDC hdc, int x, int y, int size, COLORREF color);
void DrawNeonButton(HDC hdc, RECT* rect, const char* text, bool hover, ButtonStyle style);
void DrawProgressBarModern(HDC hdc, RECT* rect, float percentage, const char* statusText, const char* speedText);
void DrawStatusPanel(HDC hdc, RECT* rect, bool isOnline, int onlineCount, const char* version);
void DrawNewsPanel(HDC hdc, RECT* rect);
void DrawBannerArea(HDC hdc, RECT* rect);
