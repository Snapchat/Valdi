// Test-only link wrapper. No desktop/user preference is changed by these tests.
#include <SDL3/SDL.h>

static SDL_SystemTheme testTheme = SDL_SYSTEM_THEME_UNKNOWN;
void setTestSystemTheme(SDL_SystemTheme theme) { testTheme = theme; }
extern "C" SDL_SystemTheme __wrap_SDL_GetSystemTheme() { return testTheme; }
