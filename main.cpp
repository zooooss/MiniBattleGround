#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <time.h>

#pragma comment(lib, "Msimg32.lib")

// 컨트롤 ID
#define ID_START_BTN   101
#define ID_HOW_BTN     102
#define ID_RESTART_BTN 201
#define ID_HOME_BTN    202

// 타이머 ID
#define TIMER_MOVE 1
#define TIMER_ZONE 2


// 월드 맵 크기 (실제 전체 맵)
int worldWidth = 2000;
int worldHeight = 2000;

// 게임 화면은 플레이어 중심의 뷰포트
int viewportWidth = 550;
int viewportHeight = 540;

// 게임 상태
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER, STATE_HOWTOPLAY, STATE_GAMECLEAR};
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

// 자기장 관련 변수
int zoneStartCountdown = 3;  // 자기장 생성까지 남은 시간
bool zoneActive = false;      // 자기장 활성화 여부
int zoneCenterX = 0;          // 자기장 중심 X (랜덤)
int zoneCenterY = 0;          // 자기장 중심 Y (랜덤)

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

//미니맵 전용 사진
HBITMAP hMinimapBmp = (HBITMAP)LoadImage(NULL, _T("minimap.bmp"), IMAGE_BITMAP, 150, 150, LR_LOADFROMFILE);

// 총알 구조체
struct Bullet {
    float x, y;           // 월드 좌표
    float vx, vy;         // 속도
    bool active;          // 활성화 여부
};

// 아이템 구조체
struct Item {
    int x, y;             // 월드 좌표
    int type;             // 0: 구급상자, 1: 에너지드링크
    bool active;
};

// 총알 & 아이템 배열
#define MAX_BULLETS 50
#define MAX_ITEMS 30
Bullet bullets[MAX_BULLETS];
Item items[MAX_ITEMS];

// 플레이어 속도
int playerSpeed = 6;
int speedBoostTimer = 0;  // 속도 증가 지속 시간

// 이미지 핸들 (기존 이미지 핸들 근처에 추가)
HBITMAP hBulletBmp = NULL;
HBITMAP hMedkitBmp = NULL;
HBITMAP hEnergyDrinkBmp = NULL;

// 총알 발사 타이머
#define TIMER_BULLET_SPAWN 3
int bulletSpawnTick = 0;

// 유틸 함수
double Distance(int x1, int y1, int x2, int y2) {
    return sqrt((double)(x1 - x2) * (x1 - x2) + (double)(y1 - y2) * (y1 - y2));
}

// How To Play 화면 렌더
void RenderHowToPlay(HDC hdc, RECT& client) {
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

    SetBkMode(hdc, TRANSPARENT);

    // 타이틀
    SetTextColor(hdc, RGB(255, 200, 50));
    HFONT hTitleFont = CreateFont(40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    HFONT oldFont = (HFONT)SelectObject(hdc, hTitleFont);
    TextOut(hdc, 250, 40, _T("HOW TO PLAY"), lstrlen(_T("HOW TO PLAY")));
    SelectObject(hdc, oldFont);
    DeleteObject(hTitleFont);

    // 설명 텍스트
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    oldFont = (HFONT)SelectObject(hdc, hFont);

    int y = 120;
    int lineHeight = 35;

    // 기본 조작
    SetTextColor(hdc, RGB(100, 200, 255));
    HFONT hBold = CreateFont(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    SelectObject(hdc, hBold);
    TextOut(hdc, 80, y, _T("Controls"), lstrlen(_T("Controls")));
    y += lineHeight;

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, 100, y, _T("W / A / S / D  -  Move your character"), lstrlen(_T("W / A / S / D  -  Move your character")));
    y += lineHeight + 10;

    // 자기장
    SelectObject(hdc, hBold);
    SetTextColor(hdc, RGB(0, 180, 255));
    TextOut(hdc, 80, y, _T("Blue Zone"), lstrlen(_T("Blue Zone")));
    y += lineHeight;

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, 100, y, _T("Zone spawns randomly 3 seconds after game starts"), lstrlen(_T("Zone spawns randomly 3 seconds after game starts")));
    y += lineHeight;
    TextOut(hdc, 100, y, _T("Shrinks every 3 seconds"), lstrlen(_T("Shrinks every 3 seconds")));
    y += lineHeight;
    TextOut(hdc, 100, y, _T("Take 5 HP damage per second outside the zone"), lstrlen(_T("Take 5 HP damage per second outside the zone")));
    y += lineHeight + 10;

    // 총알
    SelectObject(hdc, hBold);
    SetTextColor(hdc, RGB(255, 100, 100));
    TextOut(hdc, 80, y, _T("Bullets"), lstrlen(_T("Bullets")));
    y += lineHeight;

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, 100, y, _T("Spawn from map edges every 2 seconds"), lstrlen(_T("Spawn from map edges every 2 seconds")));
    y += lineHeight;
    TextOut(hdc, 100, y, _T("Headshot  -  50 HP damage"), lstrlen(_T("Headshot  -  50 HP damage")));
    y += lineHeight;
    TextOut(hdc, 100, y, _T("Body shot  -  30 HP damage"), lstrlen(_T("Body shot  -  30 HP damage")));
    y += lineHeight + 10;

    // 아이템
    SelectObject(hdc, hBold);
    SetTextColor(hdc, RGB(100, 255, 100));
    TextOut(hdc, 80, y, _T("Items"), lstrlen(_T("Items")));
    y += lineHeight;

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, 100, y, _T("Medkit (Green)  -  Restore 10 HP"), lstrlen(_T("Medkit (Green)  -  Restore 10 HP")));
    y += lineHeight;
    TextOut(hdc, 100, y, _T("Energy Drink (Orange)  -  Speed boost for 5 seconds"), lstrlen(_T("Energy Drink (Orange)  -  Speed boost for 5 seconds")));
    y += lineHeight + 10;

    // 목표
    SelectObject(hdc, hBold);
    SetTextColor(hdc, RGB(255, 200, 50));
    TextOut(hdc, 80, y, _T("Goal"), lstrlen(_T("Goal")));
    y += lineHeight;

    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, 100, y, _T("Survive as long as possible!"), lstrlen(_T("Survive as long as possible!")));
    y += lineHeight + 20;

    // BACK 안내
    SetTextColor(hdc, RGB(150, 150, 150));
    HFONT hSmall = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    SelectObject(hdc, hSmall);
    TextOut(hdc, 560, 560, _T("Press ESC to go back"), lstrlen(_T("Press ESC to go back")));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    DeleteObject(hBold);
    DeleteObject(hSmall);
}

// 총알 생성 (플레이어를 향해)
void SpawnBullet() {
    // 빈 슬롯 찾기
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            // 맵 가장자리에서 랜덤 생성
            int edge = rand() % 4;  // 0:상, 1:하, 2:좌, 3:우

            if (edge == 0) {  // 상단
                bullets[i].x = (float)(rand() % worldWidth);
                bullets[i].y = 0;
            }
            else if (edge == 1) {  // 하단
                bullets[i].x = (float)(rand() % worldWidth);
                bullets[i].y = (float)worldHeight;
            }
            else if (edge == 2) {  // 좌측
                bullets[i].x = 0;
                bullets[i].y = (float)(rand() % worldHeight);
            }
            else {  // 우측
                bullets[i].x = (float)worldWidth;
                bullets[i].y = (float)(rand() % worldHeight);
            }

            // 플레이어를 향하는 방향 계산
            float dx = playerX - bullets[i].x;
            float dy = playerY - bullets[i].y;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist > 0) {
                bullets[i].vx = (dx / dist) * 8.0f;  // 속도
                bullets[i].vy = (dy / dist) * 8.0f;
            }

            bullets[i].active = true;
            break;
        }
    }
}

// 총알 업데이트
void UpdateBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].x += bullets[i].vx;
            bullets[i].y += bullets[i].vy;

            // 맵 밖으로 나가면 비활성화
            if (bullets[i].x < 0 || bullets[i].x > worldWidth ||
                bullets[i].y < 0 || bullets[i].y > worldHeight) {
                bullets[i].active = false;
            }

            // 플레이어와 충돌 체크
            float dx = bullets[i].x - playerX;
            float dy = bullets[i].y - playerY;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist < 20) {  // 충돌
                // 머리(상단 1/3) vs 몸통
                if (dy < -8) {  // 머리
                    hp -= 30;
                }
                else {  // 몸통
                    hp -= 15;
                }
                if (hp < 0) hp = 0;
                bullets[i].active = false;
            }
        }
    }
}

// 아이템 획득 체크
void CheckItemPickup() {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            float dx = (float)(items[i].x - playerX);
            float dy = (float)(items[i].y - playerY);
            float dist = sqrt(dx * dx + dy * dy);

            if (dist < 25) {  // 획득 범위
                if (items[i].type == 0) {  // 구급상자
                    hp += 15;
                    if (hp > 100) hp = 100;
                }
                else {  // 에너지드링크
                    playerSpeed = 20;
                    speedBoostTimer = 7;  // 7초간 지속
                }
                items[i].active = false;
            }
        }
    }
}

void InitGame() {
    // 플레이어는 월드 맵 중심에서 시작
    playerX = worldWidth / 2;
    playerY = worldHeight / 2;
    playerSize = 12;
    hp = 100;
    playerSpeed = 6;
    speedBoostTimer = 0;

    // 자기장 초기화
    zoneStartCountdown = 3;
    zoneActive = false;

    int minX = worldWidth / 4;
    int maxX = worldWidth * 3 / 4;
    int minY = worldHeight / 4;
    int maxY = worldHeight * 3 / 4;

    zoneCenterX = minX + (rand() % (maxX - minX));
    zoneCenterY = minY + (rand() % (maxY - minY));

    safeRadius = worldWidth / 4;
    secondsSurvived = 0;
    shrinkTick = 0;
    bulletSpawnTick = 0;

    // 총알 초기화
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = false;
    }

    // 아이템 랜덤 생성 (10개)
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (i < 10) {
            items[i].x = 100 + (rand() % (worldWidth - 200));
            items[i].y = 100 + (rand() % (worldHeight - 200));
            items[i].type = rand() % 2;  // 0: 구급상자, 1: 에너지드링크
            items[i].active = true;
        }
        else {
            items[i].active = false;
        }
    }
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
void RenderMinimapBuffer() {
    if (!hMinimapDC) return;
    int mapSize = 150;

    // 1. 미니맵 배경 이미지
    if (hMinimapBmp) {
        HDC imgDC = CreateCompatibleDC(hMinimapDC);
        HBITMAP oldBmp = (HBITMAP)SelectObject(imgDC, hMinimapBmp);
        BitBlt(hMinimapDC, 0, 0, mapSize, mapSize, imgDC, 0, 0, SRCCOPY);
        SelectObject(imgDC, oldBmp);
        DeleteDC(imgDC);
    }

    // 2. 아이템 표시
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            float ix = (items[i].x / (float)worldWidth) * mapSize;
            float iy = (items[i].y / (float)worldHeight) * mapSize;

            COLORREF color = (items[i].type == 0) ? RGB(0, 255, 0) : RGB(255, 150, 0);
            HBRUSH itemBrush = CreateSolidBrush(color);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hMinimapDC, itemBrush);
            Ellipse(hMinimapDC, (int)ix - 2, (int)iy - 2, (int)ix + 2, (int)iy + 2);
            SelectObject(hMinimapDC, oldBrush);
            DeleteObject(itemBrush);
        }
    }

    // 3. 총알 표시
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            float bx = (bullets[i].x / (float)worldWidth) * mapSize;
            float by = (bullets[i].y / (float)worldHeight) * mapSize;

            HBRUSH bulletBrush = CreateSolidBrush(RGB(255, 0, 0));
            HBRUSH oldBrush = (HBRUSH)SelectObject(hMinimapDC, bulletBrush);
            Ellipse(hMinimapDC, (int)bx - 1, (int)by - 1, (int)bx + 1, (int)by + 1);
            SelectObject(hMinimapDC, oldBrush);
            DeleteObject(bulletBrush);
        }
    }

    // 4. 자기장
    if (zoneActive) {
        float zoneCenterMapX = (zoneCenterX / (float)worldWidth) * mapSize;
        float zoneCenterMapY = (zoneCenterY / (float)worldHeight) * mapSize;
        float zoneRadiusRatio = safeRadius / (float)worldWidth;
        int zoneR = (int)(zoneRadiusRatio * mapSize);

        HPEN zonePen = CreatePen(PS_SOLID, 3, RGB(0, 120, 255));
        HPEN oldPen = (HPEN)SelectObject(hMinimapDC, zonePen);
        SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
        Ellipse(hMinimapDC,
            (int)(zoneCenterMapX - zoneR),
            (int)(zoneCenterMapY - zoneR),
            (int)(zoneCenterMapX + zoneR),
            (int)(zoneCenterMapY + zoneR));
        SelectObject(hMinimapDC, oldPen);
        DeleteObject(zonePen);
    }

    // 5. 플레이어
    float px = (playerX / (float)worldWidth) * mapSize;
    float py = (playerY / (float)worldHeight) * mapSize;

    HBRUSH pBrush = CreateSolidBrush(RGB(255, 220, 0));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hMinimapDC, pBrush);
    Ellipse(hMinimapDC, (int)px - 5, (int)py - 5, (int)px + 5, (int)py + 5);
    SelectObject(hMinimapDC, oldBrush);
    DeleteObject(pBrush);

    HPEN playerPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hMinimapDC, playerPen);
    SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
    Ellipse(hMinimapDC, (int)px - 5, (int)py - 5, (int)px + 5, (int)py + 5);
    SelectObject(hMinimapDC, oldPen);
    DeleteObject(playerPen);

    // 6. 테두리
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(80, 90, 75));
    oldPen = (HPEN)SelectObject(hMinimapDC, borderPen);
    SelectObject(hMinimapDC, GetStockObject(NULL_BRUSH));
    Rectangle(hMinimapDC, 0, 0, mapSize, mapSize);
    SelectObject(hMinimapDC, oldPen);
    DeleteObject(borderPen);
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

    // 카메라 계산 (플레이어 중심)
    int cameraX = playerX - viewportWidth / 2;
    int cameraY = playerY - viewportHeight / 2;

    // 카메라가 월드 밖으로 나가지 않도록 제한
    if (cameraX < 0) cameraX = 0;
    if (cameraY < 0) cameraY = 0;
    if (cameraX + viewportWidth > worldWidth) cameraX = worldWidth - viewportWidth;
    if (cameraY + viewportHeight > worldHeight) cameraY = worldHeight - viewportHeight;

    // 배경 그리기 (카메라 영역만)
    HDC bgDC = CreateCompatibleDC(memDC);
    HBITMAP oldBg = (HBITMAP)SelectObject(bgDC, hBackgroundBmp);

    // 월드 맵의 일부만 잘라서 표시
    StretchBlt(memDC,
        gameRect.left, gameRect.top,
        viewportWidth, viewportHeight,
        bgDC,
        (cameraX * 1000) / worldWidth, (cameraY * 562) / worldHeight,
        (viewportWidth * 1000) / worldWidth, (viewportHeight * 562) / worldHeight,
        SRCCOPY);

    SelectObject(bgDC, oldBg);
    DeleteDC(bgDC);

    //클리핑 설정
    HRGN gameRgn = CreateRectRgn(gameRect.left, gameRect.top, gameRect.right, gameRect.bottom);
    SelectClipRgn(memDC, gameRgn);

    // 자기장 - 월드 좌표를 화면 좌표로 변환
    int worldCenterX = worldWidth / 2;
    int worldCenterY = worldHeight / 2;

    // 자기장 - 활성화된 경우에만 표시
    if (zoneActive) {
        int screenZoneCenterX = gameRect.left + (zoneCenterX - cameraX);
        int screenZoneCenterY = gameRect.top + (zoneCenterY - cameraY);

        HPEN zonePen = CreatePen(PS_SOLID, 4, RGB(0, 120, 255));
        oldPen = (HPEN)SelectObject(memDC, zonePen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Ellipse(memDC,
            screenZoneCenterX - (int)safeRadius,
            screenZoneCenterY - (int)safeRadius,
            screenZoneCenterX + (int)safeRadius,
            screenZoneCenterY + (int)safeRadius);
        SelectObject(memDC, oldPen);
        DeleteObject(zonePen);
    }

    // 클리핑 해제
    SelectClipRgn(memDC, NULL);

    // 아이템 그리기
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) {
            int screenItemX = gameRect.left + (items[i].x - cameraX);
            int screenItemY = gameRect.top + (items[i].y - cameraY);

            // 화면 안에 있을 때만 그리기
            if (screenItemX >= gameRect.left && screenItemX <= gameRect.right &&
                screenItemY >= gameRect.top && screenItemY <= gameRect.bottom) {

                HBITMAP itemBmp = (items[i].type == 0) ? hMedkitBmp : hEnergyDrinkBmp;
                if (itemBmp) {
                    HDC itemDC = CreateCompatibleDC(memDC);
                    HBITMAP oldItem = (HBITMAP)SelectObject(itemDC, itemBmp);

                    TransparentBlt(memDC,
                        screenItemX - 12, screenItemY - 12,
                        24, 24,
                        itemDC, 0, 0, 24, 24,
                        RGB(255, 0, 255));

                    SelectObject(itemDC, oldItem);
                    DeleteDC(itemDC);
                }
            }
        }
    }

    // 총알 그리기
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            int screenBulletX = gameRect.left + ((int)bullets[i].x - cameraX);
            int screenBulletY = gameRect.top + ((int)bullets[i].y - cameraY);

            if (screenBulletX >= gameRect.left && screenBulletX <= gameRect.right &&
                screenBulletY >= gameRect.top && screenBulletY <= gameRect.bottom) {

                if (hBulletBmp) {
                    HDC bulletDC = CreateCompatibleDC(memDC);
                    HBITMAP oldBullet = (HBITMAP)SelectObject(bulletDC, hBulletBmp);

                    TransparentBlt(memDC,
                        screenBulletX - 8, screenBulletY - 8,
                        16, 16,
                        bulletDC, 0, 0, 16, 16,
                        RGB(255, 0, 255));

                    SelectObject(bulletDC, oldBullet);
                    DeleteDC(bulletDC);
                }
            }
        }
    }

    // 플레이어 - 월드 좌표를 화면 좌표로 변환
    int screenPlayerX = gameRect.left + (playerX - cameraX);
    int screenPlayerY = gameRect.top + (playerY - cameraY);

    if (hPlayerBmp) {
        HDC playerDC = CreateCompatibleDC(memDC);
        HBITMAP oldPlayer = (HBITMAP)SelectObject(playerDC, hPlayerBmp);

        TransparentBlt(memDC,
            screenPlayerX - 16, screenPlayerY - 16,
            32, 32,
            playerDC, 0, 0, 32, 32,
            RGB(255, 0, 255));

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
    if (!zoneActive && zoneStartCountdown > 0) {
        // 카운트다운 표시
        SetTextColor(memDC, RGB(255, 200, 50));
        HFONT hBigFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
        HFONT oldBigFont = (HFONT)SelectObject(memDC, hBigFont);

        wsprintf(buf, _T("ZONE IN: %d"), zoneStartCountdown);
        TextOut(memDC, infoRect.left + 20, infoRect.top + 140, buf, lstrlen(buf));

        SelectObject(memDC, oldBigFont);
        DeleteObject(hBigFont);
    }
    else if (zoneActive) {
        double d = Distance(playerX, playerY, zoneCenterX, zoneCenterY);
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
    }
    // 속도 부스트 표시
    if (speedBoostTimer > 0) {
        SetTextColor(memDC, RGB(255, 150, 0));
        wsprintf(buf, _T("SPEED BOOST: %ds"), speedBoostTimer);
        TextOut(memDC, infoRect.left + 20, infoRect.top + 170, buf, lstrlen(buf));
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

void CreateGameClearPopup(HWND hWnd) {
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

    TextOut(hdc, r.left + 70, r.top + 20, _T("GAME OVER"), lstrlen(_T("GAME OVER")));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    // Congratulations 텍스트
    SetTextColor(hdc, RGB(255, 200, 50));
    HFONT hFont2 = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    oldFont = (HFONT)SelectObject(hdc, hFont2);

    TextOut(hdc, r.left + 60, r.top + 60, _T("Congratulations!"), lstrlen(_T("Congratulations!")));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont2);

    // Score 텍스트
    SetTextColor(hdc, RGB(200, 200, 200));
    HFONT hFont3 = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    oldFont = (HFONT)SelectObject(hdc, hFont3);

    TCHAR buf[128];
    wsprintf(buf, _T("Your score is %d seconds!"), secondsSurvived);
    TextOut(hdc, r.left + 70, r.top + 88, buf, lstrlen(buf));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont3);
}

void RenderGameClearPopup(HDC hdc) {
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
    HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(50, 255, 50));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    // 텍스트
    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, RGB(50, 255, 50));
    HFONT hFont = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    TextOut(hdc, r.left + 40, r.top + 20, _T("VICTORY!"), lstrlen(_T("VICTORY!")));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    // 시간/점수
    SetTextColor(hdc, RGB(200, 200, 255));
    HFONT hFont2 = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, _T("Arial"));
    oldFont = (HFONT)SelectObject(hdc, hFont2);

    TCHAR buf[128];
    wsprintf(buf, _T("Survived: %d seconds"), secondsSurvived);
    TextOut(hdc, r.left + 70, r.top + 70, buf, lstrlen(buf));

    SelectObject(hdc, oldFont);
    DeleteObject(hFont2);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hPlayerBmp = (HBITMAP)LoadImage(NULL, _T("character.bmp"), IMAGE_BITMAP, 32, 32, LR_LOADFROMFILE);
        hBackgroundBmp = (HBITMAP)LoadImage(NULL, _T("background.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        hBulletBmp = (HBITMAP)LoadImage(NULL, _T("tang.bmp"), IMAGE_BITMAP, 16, 16, LR_LOADFROMFILE);
        hMedkitBmp = (HBITMAP)LoadImage(NULL, _T("heal.bmp"), IMAGE_BITMAP, 24, 24, LR_LOADFROMFILE);
        hEnergyDrinkBmp = (HBITMAP)LoadImage(NULL, _T("energydrink.bmp"), IMAGE_BITMAP, 24, 24, LR_LOADFROMFILE);

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
            ShowMenuButtons(hWnd, false);
            g_state = STATE_HOWTOPLAY;
            InvalidateRect(hWnd, NULL, TRUE);
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
            case 'W': case 'w': playerY -= playerSpeed; break;
            case 'S': case 's': playerY += playerSpeed; break;
            case 'A': case 'a': playerX -= playerSpeed; break;
            case 'D': case 'd': playerX += playerSpeed; break;
            }

            // 월드 맵 경계 체크
            if (playerX - playerSize < 0) playerX = playerSize;
            if (playerX + playerSize > worldWidth) playerX = worldWidth - playerSize;
            if (playerY - playerSize < 0) playerY = playerSize;
            if (playerY + playerSize > worldHeight) playerY = worldHeight - playerSize;

            InvalidateRect(hWnd, NULL, FALSE);
        }
        else if (g_state == STATE_HOWTOPLAY && wParam == VK_ESCAPE) {
            // HOW TO PLAY에서 ESC 누르면 메뉴로
            ShowMenuButtons(hWnd, true);
            g_state = STATE_MENU;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;

    case WM_TIMER:
        if (g_state == STATE_PLAYING) {
            if (wParam == TIMER_ZONE) {
                secondsSurvived++;

                // 속도 부스트 타이머
                if (speedBoostTimer > 0) {
                    speedBoostTimer--;
                    if (speedBoostTimer == 0) {
                        playerSpeed = 6;  // 원래 속도로
                    }
                }

                // 자기장 카운트다운
                if (!zoneActive) {
                    zoneStartCountdown--;
                    if (zoneStartCountdown <= 0) {
                        zoneActive = true;
                    }
                }
                else {
                    shrinkTick++;
                    if (shrinkTick % 3 == 0 && safeRadius > 30) {
                        int dynamicShrink = 15 + (secondsSurvived / 5); // 시간 기반 증가
                        safeRadius -= dynamicShrink;
                    }
                    if (safeRadius <= 30 && hp > 0) {
                        KillTimer(hWnd, TIMER_MOVE);
                        KillTimer(hWnd, TIMER_ZONE);
                        g_state = STATE_GAMECLEAR;
                        CreateGameClearPopup(hWnd);
                    }
                    double d = Distance(playerX, playerY, zoneCenterX, zoneCenterY);
                    if (d > safeRadius) {
                        hp -= 5;
                        if (hp < 0) hp = 0;
                    }
                }

                // 총알 생성 (2초마다)
                bulletSpawnTick++;
                if (bulletSpawnTick >= 2 && zoneActive) {
                    SpawnBullet();
                    bulletSpawnTick = 0;
                }

                if (hp <= 0) {
                    KillTimer(hWnd, TIMER_MOVE);
                    KillTimer(hWnd, TIMER_ZONE);
                    g_state = STATE_GAMEOVER;
                    CreateGameOverPopup(hWnd);
                }

                InvalidateRect(hWnd, NULL, TRUE);
            }
            else if (wParam == TIMER_MOVE) {
                UpdateBullets();
                CheckItemPickup();
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        return 0;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT client;
        GetClientRect(hWnd, &client);

        if (g_state == STATE_MENU) {
            RenderMenu(hdc, client);
        }
        else if (g_state == STATE_HOWTOPLAY) {
            RenderHowToPlay(hdc, client);
        }
        else if (g_state == STATE_PLAYING) {
            RenderGameContents(hdc, client);
        }
        else if (g_state == STATE_GAMEOVER) {
            RenderGameContents(hdc, client);
            RenderGameOverPopup(hdc);
        }
        else if (g_state == STATE_GAMECLEAR) {
            RenderGameContents(hdc, client);
            RenderGameClearPopup(hdc);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (hPlayerBmp) DeleteObject(hPlayerBmp);
        if (hBackgroundBmp) DeleteObject(hBackgroundBmp);
        if (hTreeBmp) DeleteObject(hTreeBmp);
        if (hBulletBmp) DeleteObject(hBulletBmp);
        if (hMedkitBmp) DeleteObject(hMedkitBmp);
        if (hEnergyDrinkBmp) DeleteObject(hEnergyDrinkBmp);
        if (hMinimapBuffer) DeleteObject(hMinimapBuffer);
        if (hMinimapDC) DeleteDC(hMinimapDC);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // 랜덤 시드 초기화
    srand((unsigned int)time(NULL));

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