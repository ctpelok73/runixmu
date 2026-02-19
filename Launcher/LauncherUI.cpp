#include "LauncherUI.h"
#include <commctrl.h>
#include <stdio.h>
#include <math.h>

// Helper to fill gradient
static void GradientFillRect(HDC hdc, RECT* rect, COLORREF c1, COLORREF c2, bool vertical)
{
    TRIVERTEX vert[2];
    GRADIENT_RECT gRect;
    vert[0].x = rect->left;
    vert[0].y = rect->top;
    vert[0].Red = GetRValue(c1) << 8;
    vert[0].Green = GetGValue(c1) << 8;
    vert[0].Blue = GetBValue(c1) << 8;
    vert[0].Alpha = 0x0000;
    vert[1].x = rect->right;
    vert[1].y = rect->bottom;
    vert[1].Red = GetRValue(c2) << 8;
    vert[1].Green = GetGValue(c2) << 8;
    vert[1].Blue = GetBValue(c2) << 8;
    vert[1].Alpha = 0x0000;
    gRect.UpperLeft = 0;
    gRect.LowerRight = 1;
    GradientFill(hdc, vert, 2, &gRect, 1, vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}

// Helper to draw a glowing line
static void DrawGlowLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness)
{
    HPEN hPen = CreatePen(PS_SOLID, thickness, color);
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

void DrawSpaceBackground(HDC hdc, RECT* rect)
{
    // Deep Space Gradient
    GradientFillRect(hdc, rect, COL_BG_DARK, RGB(0, 0, 5), true);

    // Procedural Stars (Blue-tinted noise)
    static int starSeed = 0;
    if (starSeed == 0) starSeed = GetTickCount();
    srand(starSeed); // Stable noise

    for (int i = 0; i < 300; i++)
    {
        int x = rand() % (rect->right - rect->left);
        int y = rand() % (rect->bottom - rect->top);
        int brightness = rand() % 150 + 50;
        COLORREF starCol = RGB(brightness / 2, brightness / 2, brightness); // Blue tint
        SetPixel(hdc, x, y, starCol);
        
        // Some brighter stars
        if (i % 20 == 0)
        {
            SetPixel(hdc, x + 1, y, starCol);
            SetPixel(hdc, x - 1, y, starCol);
            SetPixel(hdc, x, y + 1, starCol);
            SetPixel(hdc, x, y - 1, starCol);
        }
    }
}

void DrawGlowingGem(HDC hdc, int x, int y, int size, COLORREF color)
{
    // Simple radial gradient approximation
    for (int i = size; i > 0; i -= 2)
    {
        float ratio = (float)i / (float)size;
        int r = GetRValue(color);
        int g = GetGValue(color);
        int b = GetBValue(color);
        
        // Center is whiter
        r = r + (int)((255 - r) * (1.0f - ratio));
        g = g + (int)((255 - g) * (1.0f - ratio));
        b = b + (int)((255 - b) * (1.0f - ratio));
        
        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        HPEN hPen = CreatePen(PS_NULL, 0, 0);
        HBRUSH hOld = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        
        Ellipse(hdc, x - i, y - i, x + i, y + i);
        
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }
}

void DrawMetallicFrame(HDC hdc, RECT* rect, int thickness)
{
    // Outer Border (Dark Metal)
    RECT r = *rect;
    HBRUSH hDark = CreateSolidBrush(COL_METAL_DARK);
    FrameRect(hdc, &r, hDark);
    DeleteObject(hDark);

    // Inner Bevel (Light Metal)
    InflateRect(&r, -1, -1);
    HBRUSH hLight = CreateSolidBrush(COL_METAL_LIGHT);
    FrameRect(hdc, &r, hLight);
    DeleteObject(hLight);

    // Main Body (Gradient Metal)
    InflateRect(&r, -1, -1);
    // Top
    RECT rTop = { r.left, r.top, r.right, r.top + thickness };
    GradientFillRect(hdc, &rTop, COL_METAL_MID, COL_METAL_DARK, true);
    // Bottom
    RECT rBot = { r.left, r.bottom - thickness, r.right, r.bottom };
    GradientFillRect(hdc, &rBot, COL_METAL_DARK, COL_METAL_MID, true);
    // Left
    RECT rLeft = { r.left, r.top, r.left + thickness, r.bottom };
    GradientFillRect(hdc, &rLeft, COL_METAL_MID, COL_METAL_DARK, false);
    // Right
    RECT rRight = { r.right - thickness, r.top, r.right, r.bottom };
    GradientFillRect(hdc, &rRight, COL_METAL_DARK, COL_METAL_MID, false);

    // Corner Ornaments (Triangles with Gems)
    int cornerSize = 20;
    POINT ptsTL[] = { {rect->left, rect->top}, {rect->left + cornerSize, rect->top}, {rect->left, rect->top + cornerSize} };
    POINT ptsTR[] = { {rect->right, rect->top}, {rect->right - cornerSize, rect->top}, {rect->right, rect->top + cornerSize} };
    POINT ptsBL[] = { {rect->left, rect->bottom}, {rect->left + cornerSize, rect->bottom}, {rect->left, rect->bottom - cornerSize} };
    POINT ptsBR[] = { {rect->right, rect->bottom}, {rect->right - cornerSize, rect->bottom}, {rect->right, rect->bottom - cornerSize} };

    HBRUSH hCorner = CreateSolidBrush(COL_METAL_DARK);
    HPEN hCornerPen = CreatePen(PS_SOLID, 1, COL_NEON_BLUE);
    HBRUSH hOld = (HBRUSH)SelectObject(hdc, hCorner);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hCornerPen);

    Polygon(hdc, ptsTL, 3);
    Polygon(hdc, ptsTR, 3);
    Polygon(hdc, ptsBL, 3);
    Polygon(hdc, ptsBR, 3);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOld);
    DeleteObject(hCorner);
    DeleteObject(hCornerPen);

    // Draw Gems
    DrawGlowingGem(hdc, rect->left + 8, rect->top + 8, 3, COL_NEON_BLUE);
    DrawGlowingGem(hdc, rect->right - 8, rect->top + 8, 3, COL_NEON_BLUE);
    DrawGlowingGem(hdc, rect->left + 8, rect->bottom - 8, 3, COL_NEON_BLUE);
    DrawGlowingGem(hdc, rect->right - 8, rect->bottom - 8, 3, COL_NEON_BLUE);
}

void DrawNeonButton(HDC hdc, RECT* rect, const char* text, bool hover, ButtonStyle style)
{
    // Background
    COLORREF c1 = hover ? COL_BUTTON_HOVER : COL_BUTTON_NORMAL;
    COLORREF c2 = RGB(GetRValue(c1)/2, GetGValue(c1)/2, GetBValue(c1)/2);
    
    if (style == ButtonStyle::Primary) // START GAME
    {
        // Bright blue gradient
        c1 = hover ? RGB(0, 150, 255) : RGB(0, 100, 200);
        c2 = RGB(0, 50, 100);
    }
    
    GradientFillRect(hdc, rect, c1, c2, true);

    // Border (Neon Glow)
    HBRUSH hBorder = CreateSolidBrush(hover ? COL_NEON_CYAN : COL_NEON_BLUE);
    FrameRect(hdc, rect, hBorder);
    DeleteObject(hBorder);

    // Inner Glow
    if (hover || style == ButtonStyle::Primary)
    {
        RECT r = *rect;
        InflateRect(&r, -1, -1);
        HBRUSH hGlow = CreateSolidBrush(COL_NEON_GLOW);
        FrameRect(hdc, &r, hGlow);
        DeleteObject(hGlow);
    }

    // Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, style == ButtonStyle::Primary ? RGB(255, 255, 255) : COL_TEXT_NORMAL);
    
    int fontSize = (style == ButtonStyle::Primary) ? 28 : 16;
    int weight = (style == ButtonStyle::Primary) ? FW_BOLD : FW_NORMAL;
    
    HFONT hFont = CreateFontA(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    DrawTextA(hdc, text, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void DrawProgressBarModern(HDC hdc, RECT* rect, float percentage, const char* statusText, const char* speedText)
{
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    // Background track
    HBRUSH hBg = CreateSolidBrush(RGB(20, 20, 25));
    FillRect(hdc, rect, hBg);
    DeleteObject(hBg);

    // Frame
    HBRUSH hFrame = CreateSolidBrush(COL_METAL_MID);
    FrameRect(hdc, rect, hFrame);
    DeleteObject(hFrame);

    // Bar
    int width = rect->right - rect->left - 4;
    int height = rect->bottom - rect->top - 4;
    int barWidth = (int)(width * (percentage / 100.0f));

    if (barWidth > 0)
    {
        RECT barRect = { rect->left + 2, rect->top + 2, rect->left + 2 + barWidth, rect->bottom - 2 };
        GradientFillRect(hdc, &barRect, COL_NEON_CYAN, COL_NEON_BLUE, false);
        
        // Shine line on top half
        RECT shineRect = barRect;
        shineRect.bottom = shineRect.top + (height / 2);
        GradientFillRect(hdc, &shineRect, RGB(255, 255, 255), COL_NEON_CYAN, true); // White to Cyan
    }

    char percentStr[32];
    sprintf_s(percentStr, "%.0f%%", percentage);
    
    RECT textRect = *rect;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_NEON_CYAN);
    
    int percentFontSize = (rect->bottom - rect->top) - 6;
    if (percentFontSize < 10) percentFontSize = 10;
    HFONT hFontBig = CreateFontA(percentFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT hOld = (HFONT)SelectObject(hdc, hFontBig);
    DrawTextA(hdc, percentStr, -1, &textRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
    DeleteObject(hFontBig);

    // Status Text (Below bar)
    RECT statusRect = *rect;
    statusRect.top = rect->bottom + 5;
    statusRect.bottom = statusRect.top + 20;
    
    SetTextColor(hdc, COL_TEXT_NORMAL);
    HFONT hFontSmall = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    hOld = (HFONT)SelectObject(hdc, hFontSmall);
    
    DrawTextA(hdc, statusText, -1, &statusRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    
    // Speed (Below Status)
    if (speedText && speedText[0])
    {
        RECT speedRect = statusRect;
        speedRect.top += 15;
        speedRect.bottom += 15;
        DrawTextA(hdc, speedText, -1, &speedRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    
    SelectObject(hdc, hOld);
    DeleteObject(hFontSmall);
}

void DrawBannerArea(HDC hdc, RECT* rect)
{
    // Frame
    DrawMetallicFrame(hdc, rect, 4);

    // Content (Placeholder Gradient)
    RECT content = *rect;
    InflateRect(&content, -4, -4);
    
    // Simulating an image with a rich gradient
    GradientFillRect(hdc, &content, RGB(50, 20, 10), RGB(10, 5, 5), true); // Dark reddish/brown placeholder for Castle Siege

    // Text Overlay
    RECT textRect = content;
    textRect.top = textRect.bottom - 40;
    textRect.left += 20;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 200, 100)); // Gold text
    
    HFONT hFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    
    DrawTextA(hdc, "Castle Siege Sunday", -1, &textRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
}

void DrawNewsPanel(HDC hdc, RECT* rect)
{
    // Header
    RECT headerRect = { rect->left, rect->top, rect->right, rect->top + 30 };
    HBRUSH hHeaderBg = CreateSolidBrush(COL_BUTTON_NORMAL);
    FillRect(hdc, &headerRect, hHeaderBg);
    DeleteObject(hHeaderBg);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_NEON_CYAN);
    HFONT hFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    DrawTextA(hdc, "News", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    // Body
    RECT bodyRect = { rect->left, rect->top + 30, rect->right, rect->bottom };
    HBRUSH hBodyBg = CreateSolidBrush(RGB(10, 10, 15));
    FillRect(hdc, &bodyRect, hBodyBg);
    DeleteObject(hBodyBg);
    
    // Items
    const char* news[] = {
        "[19.02] Added new map: Arena Ice",
        "[18.02] Improved drop system",
        "[17.02] New Excellent Sets",
        "[15.02] Server Maintenance"
    };
    
    SetTextColor(hdc, COL_TEXT_NORMAL);
    SelectObject(hdc, hOld); // Restore font if needed, but we want smaller normal font
    DeleteObject(hFont);
    
    HFONT hListFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    hOld = (HFONT)SelectObject(hdc, hListFont);
    
    int y = bodyRect.top + 10;
    for (int i = 0; i < 4; i++)
    {
        RECT itemRect = { bodyRect.left + 10, y, bodyRect.right - 10, y + 20 };
        DrawTextA(hdc, news[i], -1, &itemRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
        y += 25;
    }
    
    SelectObject(hdc, hOld);
    DeleteObject(hListFont);
    
    // Border around panel
    DrawMetallicFrame(hdc, rect, 2);
}

void DrawStatusPanel(HDC hdc, RECT* rect, bool isOnline, int onlineCount, const char* version)
{
    SetBkMode(hdc, TRANSPARENT);
    
    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    
    // Draw "Server Status: "
    const char* text1 = "Server Status: ";
    SIZE size1;
    GetTextExtentPoint32A(hdc, text1, strlen(text1), &size1);
    
    // Draw "Online" / "Offline"
    const char* text2 = isOnline ? "Online" : "Offline";
    SIZE size2;
    GetTextExtentPoint32A(hdc, text2, strlen(text2), &size2);
    
    // Draw "   |   Client Version: vX.XX"
    char text3[64];
    sprintf_s(text3, "   |   Client Version: v%s", version);
    SIZE size3;
    GetTextExtentPoint32A(hdc, text3, strlen(text3), &size3);
    
    int totalWidth = size1.cx + size2.cx + size3.cx;
    int startX = rect->left + (rect->right - rect->left - totalWidth) / 2;
    int y = rect->top + (rect->bottom - rect->top - size1.cy) / 2;
    
    // 1
    SetTextColor(hdc, COL_TEXT_NORMAL);
    TextOutA(hdc, startX, y, text1, strlen(text1));
    
    // 2 (Green for Online, Red for Offline)
    SetTextColor(hdc, isOnline ? RGB(0, 255, 0) : RGB(255, 0, 0));
    TextOutA(hdc, startX + size1.cx, y, text2, strlen(text2));
    
    // 3
    SetTextColor(hdc, COL_TEXT_NORMAL);
    TextOutA(hdc, startX + size1.cx + size2.cx, y, text3, strlen(text3));
    
    SelectObject(hdc, hOld);
    DeleteObject(hFont);
}
