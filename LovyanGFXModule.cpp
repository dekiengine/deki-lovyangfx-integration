/**
 * @file LovyanGFXModule.cpp
 * @brief Module entry point for deki-lovyangfx DLL
 *
 * This file exports the standard Deki plugin interface so the editor
 * can load deki-lovyangfx.dll and discover available LovyanGFX components.
 *
 * For linked DLLs (not dynamically loaded), DekiLovyanGFX_EnsureRegistered()
 * must be called from the main executable to trigger the static initializers.
 */

#include "LovyanGFXModule.h"
#include "interop/DekiPlugin.h"
#include "modules/DekiModuleFeatureMeta.h"
#include "LGFXDisplayPanel.h"
#include "LGFXTouchPanel.h"
#include "reflection/ComponentRegistry.h"
#include "reflection/ComponentFactory.h"

#ifdef DEKI_EDITOR

// Auto-generated registration helpers
extern void DekiLovyanGFX_RegisterComponents();
extern int DekiLovyanGFX_GetAutoComponentCount();
extern const DekiComponentMeta* DekiLovyanGFX_GetAutoComponentMeta(int index);

// Track if already registered to avoid duplicates
static bool s_LovyanGFXRegistered = false;

extern "C" {

/**
 * @brief Ensure deki-lovyangfx module is loaded and components are registered
 *
 * Call this from the editor at startup. Simply calling this function is enough
 * to force the linker to include the DLL and trigger static initializers.
 *
 * @return Number of components registered by this module
 */
DEKI_LOVYANGFX_API int DekiLovyanGFX_EnsureRegistered(void)
{
    if (s_LovyanGFXRegistered)
        return DekiLovyanGFX_GetAutoComponentCount();
    s_LovyanGFXRegistered = true;

    // Auto-generated: registers all LovyanGFX components with ComponentRegistry + ComponentFactory
    DekiLovyanGFX_RegisterComponents();

    return DekiLovyanGFX_GetAutoComponentCount();
}

// =============================================================================
// Plugin metadata (for dynamic loading compatibility)
// =============================================================================

DEKI_PLUGIN_API const char* DekiPlugin_GetName(void)
{
    return "Deki LovyanGFX Module";
}

DEKI_PLUGIN_API const char* DekiPlugin_GetVersion(void)
{
#ifdef DEKI_MODULE_VERSION
    return DEKI_MODULE_VERSION;
#else
    return "0.0.0-dev";
#endif
}

DEKI_PLUGIN_API const char* DekiPlugin_GetReflectionJson(void)
{
    return "{}";
}

DEKI_PLUGIN_API int DekiPlugin_Init(void)
{
    return 0;
}

DEKI_PLUGIN_API void DekiPlugin_Shutdown(void)
{
    s_LovyanGFXRegistered = false;
}

DEKI_PLUGIN_API int DekiPlugin_GetComponentCount(void)
{
    return DekiLovyanGFX_GetAutoComponentCount();
}

DEKI_PLUGIN_API const DekiComponentMeta* DekiPlugin_GetComponentMeta(int index)
{
    return DekiLovyanGFX_GetAutoComponentMeta(index);
}

DEKI_PLUGIN_API void DekiPlugin_RegisterComponents(void)
{
    DekiLovyanGFX_EnsureRegistered();
}

// =============================================================================
// Module Feature API
// =============================================================================

static const DekiModuleFeatureInfo s_Features[] = {
    {"display", "Display Panel", "Configure LovyanGFX display hardware", false},
    {"touch", "Touch Panel", "Enable LovyanGFX touch panel support", false},
};

DEKI_PLUGIN_API int DekiPlugin_GetFeatureCount(void)
{
    return sizeof(s_Features) / sizeof(s_Features[0]);
}

DEKI_PLUGIN_API const DekiModuleFeatureInfo* DekiPlugin_GetFeature(int index)
{
    if (index < 0 || index >= DekiPlugin_GetFeatureCount())
        return nullptr;
    return &s_Features[index];
}

// =============================================================================
// Module-specific feature API (for linked DLL access without name conflicts)
// =============================================================================

DEKI_LOVYANGFX_API const char* DekiLovyanGFX_GetName(void)
{
    return "LovyanGFX";
}

DEKI_LOVYANGFX_API int DekiLovyanGFX_GetFeatureCount(void)
{
    return DekiPlugin_GetFeatureCount();
}

DEKI_LOVYANGFX_API const DekiModuleFeatureInfo* DekiLovyanGFX_GetFeature(int index)
{
    return DekiPlugin_GetFeature(index);
}

} // extern "C"

#else // !DEKI_EDITOR - Runtime (ESP32) registration

// For runtime builds, component registration happens via static initializers
// or explicit calls from the application

#endif // DEKI_EDITOR
