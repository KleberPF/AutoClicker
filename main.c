#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// screen dimensions
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 900

// coordinates of the top left square
#define TOP_LEFT_X 612
#define TOP_LEFT_Y 270

// offset between two squares
#define SQUARE_OFFSET 150

// delay between clicks (in ms)
#define CLICK_DELAY 1

// delay between screenshots
#define SCRSHOT_DELAY 1

void mouseSetup(INPUT* input)
{
    input->type = INPUT_MOUSE;
    input->mi.mouseData = 0;
    input->mi.time = 0;
    input->mi.dwExtraInfo = 0;
}

void mouseMoveAbsolute(INPUT* input, int x, int y)
{
    input->mi.dx = x * 65536 / SCREEN_WIDTH + 1;
    input->mi.dy = y * 65536 / SCREEN_HEIGHT + 1;
    input->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE);

    SendInput(1, input, sizeof(INPUT));
}

void mouseClick(INPUT* input)
{
    input->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN);
    SendInput(1, input, sizeof(INPUT));

    Sleep(10);

    input->mi.dwFlags = (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP);
    SendInput(1, input, sizeof(INPUT));
}

PBITMAPINFO createBitmapInfoStruct(HBITMAP hBmp)
{
    BITMAP bmp = { 0 };

    // retrieve the bitmap color format, width and height
    GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp);

    // convert the color format to a count of bits
    WORD clrBitsCount = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);

    if (clrBitsCount == 1) { clrBitsCount = 1; }
    else if (clrBitsCount <= 4) { clrBitsCount = 4; }
    else if (clrBitsCount <= 8) { clrBitsCount = 8; }
    else if (clrBitsCount <= 16) { clrBitsCount = 16; }
    else if (clrBitsCount <= 24) { clrBitsCount = 24; }
    else { clrBitsCount = 32; }

    // allocate memory for the BITMAPINFO struct, which contains a BITMAPINFOHEADER and an array of RGBQUAD
    PBITMAPINFO bmpInfo;
    if (clrBitsCount < 24)
    {
        bmpInfo = malloc(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * ((SIZE_T)1 << clrBitsCount));
    }
    else // there is no RGBQUAD array for 24-bit and 32-bit
    {
        bmpInfo = malloc(sizeof(BITMAPINFOHEADER));
    }

    // initialize the fields of the BITMAPINFO struct
    if (bmpInfo != NULL)
    {
        bmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmpInfo->bmiHeader.biWidth = bmp.bmWidth;
        bmpInfo->bmiHeader.biHeight = bmp.bmHeight;
        bmpInfo->bmiHeader.biPlanes = bmp.bmPlanes;
        bmpInfo->bmiHeader.biBitCount = bmp.bmBitsPixel;
        if (clrBitsCount < 24) { bmpInfo->bmiHeader.biClrUsed = (1 << clrBitsCount); }

        // if the bitmap is not compressed, set the BI_RGB flag
        bmpInfo->bmiHeader.biCompression = BI_RGB;
        bmpInfo->bmiHeader.biSizeImage = ((bmpInfo->bmiHeader.biWidth * clrBitsCount + 31) & ~31) / 8 * bmpInfo->bmiHeader.biHeight;
        bmpInfo->bmiHeader.biClrImportant = 0;
    }

    return bmpInfo;
}

DWORD* getBitmapPixelArray(HBITMAP hBmp)
{
    HDC hdc = CreateCompatibleDC(GetWindowDC(GetDesktopWindow()));
    SelectObject(hdc, hBmp);
    PBITMAPINFO bmpInfo = createBitmapInfoStruct(hBmp);

    PBITMAPINFOHEADER bmpInfoHeader = (PBITMAPINFOHEADER)bmpInfo;
    LPBYTE bmpBits = malloc(SCREEN_HEIGHT * SCREEN_WIDTH * 4);

    // retrieve the color table (RGBQUAD array) and the bits (array of pallete indices) from the DIB
    GetDIBits(hdc, hBmp, 0, (WORD)bmpInfoHeader->biHeight, bmpBits, bmpInfo, DIB_RGB_COLORS);
    free(bmpInfo);

    return (DWORD*)bmpBits;
}

DWORD getPixelColorAtCoord(DWORD* pixelArr, int x, int y)
{
    int actualY = SCREEN_HEIGHT - y - 1;
    return pixelArr[x + actualY * SCREEN_WIDTH];
}

HBITMAP getScreenBitmap()
{
    // get the device context of the screen
    HDC hScreenDC = GetDC(NULL);
    // and a device context to put it in
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, SCREEN_WIDTH, SCREEN_HEIGHT);

    // get a new bitmap
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hScreenDC, 0, 0, SRCCOPY);
    hBitmap = (HBITMAP)SelectObject(hMemoryDC, hOldBitmap);

    // clean up
    DeleteDC(hMemoryDC);
    DeleteDC(hScreenDC);

    return hBitmap;
}

void checkBlackSquares(DWORD* pixelArr, int playingField[4][4])
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            // 1 if pixel is black, 0 otherwise
            playingField[i][j] = getPixelColorAtCoord(pixelArr, TOP_LEFT_X + j * SQUARE_OFFSET, 
                TOP_LEFT_Y + i * SQUARE_OFFSET) < 0xff000011;
        }
    }
}

void clickSquares(INPUT input, int playingField[4][4])
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            if (playingField[i][j])
            {
                mouseMoveAbsolute(&input, TOP_LEFT_X + j * SQUARE_OFFSET, TOP_LEFT_Y + i * SQUARE_OFFSET);
                mouseClick(&input);
                Sleep(CLICK_DELAY);
            }
        }
    }
}

int main()
{
    INPUT input;
    mouseSetup(&input);

    bool running = true;
    while (running)
    {
        if (GetAsyncKeyState(VK_F4))
        {
            HBITMAP hBitmap = getScreenBitmap();
            DWORD* pixelArr = getBitmapPixelArray(hBitmap);

            int playingField[4][4];
            checkBlackSquares(pixelArr, playingField);
            clickSquares(input, playingField);
            Sleep(SCRSHOT_DELAY);

            free(pixelArr);
        }
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            running = false;
        }
    }
}