#pragma once

/**
 * @file LovyanGFXPackage.h
 * @brief Central header for the Deki LovyanGFX Package
 *
 * This package provides LovyanGFX-specific components:
 * - Display panel configuration (SPI/Parallel bus, panel type, backlight)
 * - Touch panel configuration (capacitive/resistive)
 */

// DLL export macro
#ifdef _WIN32
    #ifdef DEKI_LOVYANGFX_EXPORTS
        #define DEKI_LOVYANGFX_API __declspec(dllexport)
    #else
        #define DEKI_LOVYANGFX_API __declspec(dllimport)
    #endif
#else
    #define DEKI_LOVYANGFX_API __attribute__((visibility("default")))
#endif
