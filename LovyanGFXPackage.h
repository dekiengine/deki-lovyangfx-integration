#pragma once

// The LovyanGFX package's components:
// - display panel setup (SPI or parallel bus, panel type, backlight)
// - touch panel setup (capacitive or resistive)

#ifdef _WIN32
#ifdef DEKI_LOVYANGFX_EXPORTS
#define DEKI_LOVYANGFX_API __declspec(dllexport)
#else
#define DEKI_LOVYANGFX_API __declspec(dllimport)
#endif
#else
#define DEKI_LOVYANGFX_API __attribute__((visibility("default")))
#endif
