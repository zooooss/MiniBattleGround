#include <windows.h>
#include <tchar.h>
#include <math.h>

#pragma comment(lib, "Msimg32.lib")

// 컨트롤 ID
#define ID_START_BTN   101
#define ID_HOW_BTN     102
#define ID_RESTART_BTN 201
#define ID_HOME_BTN    202

// 타이머 ID
#define TIMER_MOVE 1
#define TIMER_ZONE 2

// 게임 상태
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
GameState g_state = STATE_MENU;

// 윈도우 인스턴스
HINSTANCE g_hInst = NULL;

// UI/게임 변수
int playerX = 200, playerY = 300;
int playerSize = 12;
int hp = 100;
float safeRadius = 200.0f;
float safeShrinkSpeed = 0.15f;
int secondsSurvived = 0;
int shrinkTick = 0;

// 영역(Rect)
RECT hpRect = { 0,0,800,60 };
RECT gameRect = { 0,60,550,600 };
RECT infoRect = { 550,60,800,600 };

// 메뉴 버튼
HWND hStartBtn = NULL;
HWND hHowBtn = NULL;

// 팝업
HWND hPopup = NULL;
HWND hPopupRestart = NULL;
HWND hPopupHome = NULL;

// 이미지 핸들
HBITMAP hPlayerBmp = NULL;
HBITMAP hBackgroundBmp = NULL;
HBITMAP hTreeBmp = NULL;

// 미니맵 전용 백버퍼
HBITMAP hMinimapBuffer = NULL;
HDC hMinimapDC = NULL;

// 유틸 함수
double Distance(int x1, int y1, int x2, int y2) {
    return sqrt((double)(x1 - x2) * (x1 - x2) + (double)(y1 - y2) * (y1 - y2));
}

void InitGame() {
    playerX = (gameRect.left + gameRect.right) / 2;
    playerY = (gameRect.top + gameRect.bottom) / 2;
    playerSize = 12;
    hp = 100;
    safeRadius = min((gameRect.right - gameRect.left), (gameRect.bottom - gameRect.top)) / 2.5f;
    secondsSurvived = 0;
    shrinkTick = 0;
}

// 그라데이션 원 그리기
void DrawGradientCircle(HDC hdc, int cx, int cy, float radius, COLORREF inner, COLORREF outer) {
    int steps = 30;
    for (int i = steps; i >= 0; i--) {
        float ratio = (float)i / steps;
        int r = (int)(radius * ratio);

        int r1 = GetRValue(inner), g1 = GetGValue(inner), b1 = GetBValue(inner);
        int r2 = GetRValue(outer), g2 = GetGValue(outer), b2 = GetBValue(outer);

        int rr = r1 + (int)((r2 - r1) * (1 - ratio));
        int gg = g1 + (int)((g2 - g1) * (1 - ratio));
        int bb = b1 + (int)((b2 - b1) * (1 - ratio));

        HBRUSH brush = CreateSolidBrush(RGB(rr, gg, bb));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
        SelectObject(hdc, GetStockObject(NULL_PEN));

        Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }
}

// 미니맵 백버퍼 초기화
void InitMinimapBuffer(int mapSize) {
    if (hMinimapDC) { DeleteDC(hMinimapDC); hMinimapDC = NULL; }
    if (hMinimapBuffer) { DeleteObject(hMinimapBuffer); hMinimapBuffer = NULL; }

    HDC hdc = GetDC(NULL);
    hMinimapDC = CreateCompatibleDC(hdc);
    hMinimapBuffer = CreateCompatibleBitmap(hdc, mapSize, mapSize);
    SelectObject(hMinimapDC, hMinimapBuffer);
    ReleaseDC(NULL, hdc);
}

// 미니맵 백버퍼에 그림
void RenderMinimapBuffer() {
    if (!hMinimapDC) return;

    int mapSize = 150;

    // 배경 - 어두운 그린/브라운 톤
    HBRUSH bg = CreateSolidBrush(RGB(45, 52, 40));
    HBRUSH old = (HBRUSH)SelectObject(hMinimapDC, bg);
    Rectangle(hMinimapDC, 0, 0, mapSize, mapSize);
    SelectObject(hMinimapDC, old);
    DeleteObject(bg);

    // 테두리
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(80, 90, 75));
    HPEN oldPen = (HPEN)SelectObject(hMinimapDC, borderPen);
    SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
    Rectangle(hMinimapDC, 0, 0, mapSize, mapSize);
    SelectObject(hMinimapDC, oldPen);
    DeleteObject(borderPen);

    // 자기장 - 블루존
    float worldSize = 550.0f;
    float zoneRatio = safeRadius / (worldSize / 2);
    int zoneR = (int)(zoneRatio * (mapSize / 2));

    HPEN zonePen = CreatePen(PS_SOLID, 3, RGB(0, 120, 255));
    oldPen = (HPEN)SelectObject(hMinimapDC, zonePen);
    SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
    Ellipse(hMinimapDC, mapSize / 2 - zoneR, mapSize / 2 - zoneR,
        mapSize / 2 + zoneR, mapSize / 2 + zoneR);
    SelectObject(hMinimapDC, oldPen);
    DeleteObject(zonePen);

    // 플레이어 위치
    float gameWidth = (float)(gameRect.right - gameRect.left);   // 550
    float gameHeight = (float)(gameRect.bottom - gameRect.top);  // 540
    float px = ((playerX - gameRect.left) / gameWidth) * mapSize;
    float py = ((playerY - gameRect.top) / gameHeight) * mapSize;
    HBRUSH pBrush = CreateSolidBrush(RGB(255, 220, 0));
    old = (HBRUSH)SelectObject(hMinimapDC, pBrush);
    Ellipse(hMinimapDC, (int)px - 5, (int)py - 5, (int)px + 5, (int)py + 5);
    SelectObject(hMinimapDC, old);
    DeleteObject(pBrush);

    // 플레이어 외곽선
    HPEN playerPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    oldPen = (HPEN)SelectObject(hMinimapDC, playerPen);
    SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
    Ellipse(hMinimapDC, (int)px - 5, (int)py - 5, (int)px + 5, (int)py + 5);
    SelectObject(hMinimapDC, oldPen);
    DeleteObject(playerPen);
}

// 게임 화면 렌더링
void RenderGameContents(HDC hdc, RECT client) {
    // 백버퍼 생성
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP backBuffer = CreateCompatibleBitmap(hdc,
        client.right - client.left,
        client.bottom - client.top);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, backBuffer);

    // 전체 배경
    HBRUSH bg = CreateSolidBrush(RGB(35, 35, 40));
    FillRect(memDC, &client, bg);
    DeleteObject(bg);

    // HP 바 영역 - 다크 톤
    HBRUSH hpBg = CreateSolidBrush(RGB(25, 25, 30));
    FillRect(memDC, &hpRect, hpBg);
    DeleteObject(hpBg);

    // HP 텍스트
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    HFONT hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

    TCHAR hpText[64];
    wsprintf(hpText, _T("HP: %d"), hp);
    TextOut(memDC, 20, 22, hpText, lstrlen(hpText));

    // HP 바
    int barX = 120, barY = 20, barW = 250, barH = 22;

    // 배경
    HBRUSH barBg = CreateSolidBrush(RGB(50, 50, 55));
    RECT rBar = { barX, barY, barX + barW, barY + barH };
    FillRect(memDC, &rBar, barBg);
    DeleteObject(barBg);

    // HP 그라데이션
    int fillW = (hp * barW) / 100;
    for (int i = 0; i < fillW; i++) {
        float ratio = (float)i / barW;
        int r = 220 - (int)(40 * ratio);
        int g = 20 + (int)(20 * ratio);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, 0));
        HPEN oldPen = (HPEN)SelectObject(memDC, pen);
        MoveToEx(memDC, barX + i, barY, NULL);
        LineTo(memDC, barX + i, barY + barH);
        SelectObject(memDC, oldPen);
        DeleteObject(pen);
    }

    // HP 바 테두리
    HPEN barPen = CreatePen(PS_SOLID, 2, RGB(100, 100, 110));
    HPEN oldPen = (HPEN)SelectObject(memDC, barPen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, barX, barY, barX + barW, barY + barH);
    SelectObject(memDC, oldPen);
    DeleteObject(barPen);

    //// 게임 영역 - 배경 그리기
    HDC bgDC = CreateCompatibleDC(memDC);
    HBITMAP oldBg = (HBITMAP)SelectObject(bgDC, hBackgroundBmp);

    // 배경 이미지를 게임 영역에 맞춰서 늘려서 그리기
    int gameWidth = gameRect.right - gameRect.left;   // 550
    int gameHeight = gameRect.bottom - gameRect.top;  // 540

   StretchBlt(memDC,
      gameRect.left, gameRect.top,           // 목적지 위치
        gameWidth, gameHeight,                 // 목적지 크기
        bgDC,
        0, 0,                                  // 원본 위치
        1000, 562,                             // 원본 크기
        SRCCOPY);

    SelectObject(bgDC, oldBg);
    DeleteDC(bgDC);

    // 자기장 - 블루존 (그라데이션 + 반투명 효과)
    int centerX = (gameRect.left + gameRect.right) / 2;
    int centerY = (gameRect.top + gameRect.bottom) / 2;

    // 블루존 테두리
    HPEN zonePen = CreatePen(PS_SOLID, 4, RGB(0, 120, 255));
    oldPen = (HPEN)SelectObject(memDC, zonePen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Ellipse(memDC, centerX - (int)safeRadius, centerY - (int)safeRadius,
        centerX + (int)safeRadius, centerY + (int)safeRadius);
    SelectObject(memDC, oldPen);
    DeleteObject(zonePen);

    // 플레이어
    if (hPlayerBmp) {
        HDC playerDC = CreateCompatibleDC(memDC);
        HBITMAP oldPlayer = (HBITMAP)SelectObject(playerDC, hPlayerBmp);

        TransparentBlt(memDC,
            playerX - 16, playerY - 16,
            32, 32,
            playerDC, 0, 0, 32, 32,
            RGB(255, 0, 255)); // 마젠타를 투명색으로

        SelectObject(playerDC, oldPlayer);
        DeleteDC(playerDC);
    }

    // Info 영역 - 다크 톤
    HBRUSH bInfo = CreateSolidBrush(RGB(30, 30, 35));
    FillRect(memDC, &infoRect, bInfo);
    DeleteObject(bInfo);

    // Info 텍스트
    SetTextColor(memDC, RGB(200, 200, 200));
    TCHAR buf[128];

    TextOut(memDC, infoRect.left + 20, infoRect.top + 20, _T("GAME INFO"), lstrlen(_T("GAME INFO")));

    SetTextColor(memDC, RGB(255, 255, 255));
    swprintf_s(buf, 128, L"Zone: %.1f", safeRadius);
    TextOut(memDC, infoRect.left + 20, infoRect.top + 50, buf, lstrlen(buf));

    wsprintf(buf, _T("Time: %d sec"), secondsSurvived);
    TextOut(memDC, infoRect.left + 20, infoRect.top + 80, buf, lstrlen(buf));

    // 상태 표시
    double d = Distance(playerX, playerY, centerX, centerY);
    if (d > safeRadius) {
        SetTextColor(memDC, RGB(255, 80, 80));
        TextOut(memDC, infoRect.left + 20, infoRect.top + 110,
            _T("OUT OF ZONE!"), lstrlen(_T("OUT OF ZONE!")));
    }
    else {
        SetTextColor(memDC, RGB(100, 255, 100));
        TextOut(memDC, infoRect.left + 20, infoRect.top + 110,
            _T("Safe"), lstrlen(_T("Safe")));
    }

    // 미니맵 타이틀
    SetTextColor(memDC, RGB(200, 200, 200));
    TextOut(memDC, infoRect.left + 20, infoRect.top + 200,
        _T("MINIMAP"), lstrlen(_T("MINIMAP")));

    // 미니맵
    RenderMinimapBuffer();
    int mapSize = 150;
    int mapX = infoRect.left + 20;
    int mapY = infoRect.top + 230;
    BitBlt(memDC, mapX, mapY, mapSize, mapSize, hMinimapDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);

    // 화면 출력
    BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top,
        memDC, 0, 0, SRCCOPY);

    // 정리
    SelectObject(memDC, oldBmp);
    DeleteObject(backBuffer);
    DeleteDC(memDC);
}

// 팝업
void CreateGameOverPopup(HWND hWnd) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int popupW = 350, popupH = 180;
    int px = (rcClient.right - popupW) / 2;
    int py = (rcClient.bottom - popupH) / 2;

    hPopup = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        _T("STATIC"),
        NULL,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        px, py, popupW, popupH,
        hWnd, NULL, g_hInst, NULL
    );

    hPopupRestart = CreateWindow(
        _T("BUTTON"), _T("RESTART"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        px + 50, py + 120, 100, 35,
        hWnd, (HMENU)ID_RESTART_BTN, g_hInst, NULL
    );

    hPopupHome = CreateWindow(
        _T("BUTTON"), _T("HOME"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        px + 200, py + 120, 100, 35,
        hWnd, (HMENU)ID_HOME_BTN, g_hInst, NULL
    );
}

void DestroyGameOverPopup() {
    if (hPopupRestart) { DestroyWindow(hPopupRestart); hPopupRestart = NULL; }
    if (hPopupHome) { DestroyWindow(hPopupHome); hPopupHome = NULL; }
    if (hPopup) { DestroyWindow(hPopup); hPopup = NULL; }
}


// 메뉴 버튼
void ShowMenuButtons(HWND hWnd, bool show) {
    if (show) {
        if (!hStartBtn) {
            hStartBtn = CreateWindow(_T("BUTTON"), _T("START GAME"),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                300, 250, 200, 60,
                hWnd, (HMENU)ID_START_BTN, g_hInst, NULL);
        }
        else ShowWindow(hStartBtn, SW_SHOW);

        if (!hHowBtn) {
            hHowBtn = CreateWindow(_T("BUTTON"), _T("HOW TO PLAY"),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                300, 330, 200, 60,
                hWnd, (HMENU)ID_HOW_BTN, g_hInst, NULL);
        }
        else ShowWindow(hHowBtn, SW_SHOW);
    }
    else {
        if (hStartBtn) ShowWindow(hStartBtn, SW_HIDE);
        if (hHowBtn) ShowWindow(hHowBtn, SW_HIDE);
    }
}

// 메뉴 렌더
void RenderMenu(HDC hdc, RECT& client) {
    // 그라데이션 배경
    for (int y = 0; y < client.bottom; y++) {
        float ratio = (float)y / client.bottom;
        int r = 30 + (int)(20 * ratio);
        int g = 30 + (int)(20 * ratio);
        int b = 40 + (int)(30 * ratio);

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, y, NULL);
        LineTo(hdc, client.right, y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    // 타이틀
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 200, 50));
    HFONT hTitleFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    HFONT oldFont = (HFONT)SelectObject(hdc, hTitleFont);

    TCHAR title[] = _T("MINI BATTLEGROUND");
    TextOut(hdc, 150, 120, title, lstrlen(title));

    SelectObject(hdc, oldFont);
    DeleteObject(hTitleFont);
}

// 게임오버 렌더
void RenderGameOverPopup(HDC hdc) {
    if (!hPopup) return;

    RECT rc;
    GetWindowRect(hPopup, &rc);
    POINT topleft = { rc.left, rc.top };
    ScreenToClient(GetParent(hPopup), &topleft);
    RECT r = { topleft.x, topleft.y, topleft.x + (rc.right - rc.left), topleft.y + (rc.bottom - rc.top) };

    // 반투명 배경
    HBRUSH darkBrush = CreateSolidBrush(RGB(20, 20, 25));
    FillRect(hdc, &r, darkBrush);
    DeleteObject(darkBrush);

    // 테두리
    HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(255, 80, 80));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    // 텍스트
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 80, 80));
    HFONT hFont = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    TextOut(hdc, r.left + 70, r.top + 30, _T("GAME OVER"), lstrlen(_T("GAME OVER")));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    SetTextColor(hdc, RGB(200, 200, 200));
    HFONT hFont2 = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    oldFont = (HFONT)SelectObject(hdc, hFont2);

    TCHAR buf[128];
    wsprintf(buf, _T("You survived %d seconds"), secondsSurvived);
    TextOut(hdc, r.left + 70, r.top + 75, buf, lstrlen(buf));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont2);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hPlayerBmp = (HBITMAP)LoadImage(NULL, _T("character.bmp"), IMAGE_BITMAP, 32, 32, LR_LOADFROMFILE);
        hBackgroundBmp = (HBITMAP)LoadImage(NULL, _T("background.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

        ShowMenuButtons(hWnd, true);
        InitMinimapBuffer(150);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_START_BTN && code == BN_CLICKED) {
            ShowMenuButtons(hWnd, false);
            InitGame();
            g_state = STATE_PLAYING;
            SetTimer(hWnd, TIMER_MOVE, 16, NULL);
            SetTimer(hWnd, TIMER_ZONE, 1000, NULL);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        else if (id == ID_HOW_BTN && code == BN_CLICKED) {
            MessageBox(hWnd,
                _T("WASD로 이동하세요\n\n블루존(파란 원) 안에 머물러야 합니다\n원 밖에 있으면 HP가 감소합니다\n\n최대한 오래 생존하세요!"),
                _T("How To Play"), MB_OK | MB_ICONINFORMATION);
        }
        else if (id == ID_RESTART_BTN && code == BN_CLICKED) {
            DestroyGameOverPopup();
            InitGame();
            g_state = STATE_PLAYING;
            SetTimer(hWnd, TIMER_MOVE, 16, NULL);
            SetTimer(hWnd, TIMER_ZONE, 1000, NULL);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        else if (id == ID_HOME_BTN && code == BN_CLICKED) {
            DestroyGameOverPopup();
            ShowMenuButtons(hWnd, true);
            g_state = STATE_MENU;
            InvalidateRect(hWnd, NULL, TRUE);
        }
    } return 0;

    case WM_KEYDOWN:
        if (g_state == STATE_PLAYING) {
            switch (wParam) {
            case 'W': case 'w': playerY -= 6; break;
            case 'S': case 's': playerY += 6; break;
            case 'A': case 'a': playerX -= 6; break;
            case 'D': case 'd': playerX += 6; break;
            }
            if (playerX - playerSize < gameRect.left) playerX = gameRect.left + playerSize;
            if (playerX + playerSize > gameRect.right) playerX = gameRect.right - playerSize;
            if (playerY - playerSize < gameRect.top) playerY = gameRect.top + playerSize;
            if (playerY + playerSize > gameRect.bottom) playerY = gameRect.bottom - playerSize;

            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_TIMER:
        if (g_state == STATE_PLAYING) {
            if (wParam == TIMER_ZONE) {
                shrinkTick++;
                secondsSurvived++;
                if (shrinkTick % 3 == 0 && safeRadius > 30) safeRadius -= 8;

                int cx = (gameRect.left + gameRect.right) / 2;
                int cy = (gameRect.top + gameRect.bottom) / 2;
                double d = Distance(playerX, playerY, cx, cy);
                if (d > safeRadius) { hp -= 5; if (hp < 0) hp = 0; }

                if (hp <= 0) {
                    KillTimer(hWnd, TIMER_MOVE);
                    KillTimer(hWnd, TIMER_ZONE);
                    g_state = STATE_GAMEOVER;
                    CreateGameOverPopup(hWnd);
                }

                InvalidateRect(hWnd, NULL, TRUE);
            }
            else if (wParam == TIMER_MOVE) {
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT client;
        GetClientRect(hWnd, &client);

        if (g_state == STATE_MENU) {
            RenderMenu(hdc, client);
        }
        else if (g_state == STATE_PLAYING) {
            RenderGameContents(hdc, client);
        }
        else if (g_state == STATE_GAMEOVER) {
            RenderGameContents(hdc, client);
            RenderGameOverPopup(hdc);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (hPlayerBmp) DeleteObject(hPlayerBmp);
        if (hBackgroundBmp) DeleteObject(hBackgroundBmp);
        if (hTreeBmp) DeleteObject(hTreeBmp);
        if (hMinimapBuffer) DeleteObject(hMinimapBuffer);
        if (hMinimapDC) DeleteDC(hMinimapDC);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("MiniBattleground");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(
        wc.lpszClassName,
        _T("Mini Battleground"),
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 650,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}