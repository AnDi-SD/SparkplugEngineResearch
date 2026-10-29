#include "OwnedGameProcess.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    return winxcoop::adapter::RunGameHostMain();
}
