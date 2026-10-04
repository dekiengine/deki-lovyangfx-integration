/**
 * @file LovyanGFXPackage.cpp
 * @brief Package entry point for deki-lovyangfx DLL
 *
 * This file exports the standard Deki plugin interface so the editor
 * can load deki-lovyangfx.dll and discover available LovyanGFX components.
 *
 * For linked DLLs (not dynamically loaded), DekiLovyanGFXEnsureRegistered()
 * must be called from the main executable to trigger the static initializers.
 */

#include "LovyanGFXPackage.h"
#include <deki/interop/Plugin.h>
#include "LGFXDisplayPanel.h"
#include "LGFXTouchPanel.h"
#include <deki/reflection/ComponentRegistry.h>
#include <deki/reflection/ComponentFactory.h>

extern void DekiLovyanGFXRegisterComponents();
extern int DekiLovyanGFXGetAutoComponentCount();
extern const Deki::ComponentMeta* DekiLovyanGFXGetAutoComponentMeta(int index);

namespace DekiLovyanGfx
{

#ifdef DEKI_EDITOR

// Auto-generated registration helpers

// Track if already registered to avoid duplicates
static bool s_LovyanGFXRegistered = false;

// The exports below are C symbols at global scope; the package's own
// registration helpers and statics live in its namespace.
using namespace DekiLovyanGfx;

extern "C"
{
    /**
     * @brief Ensure deki-lovyangfx package is loaded and components are registered
     *
     * Call this from the editor at startup. Simply calling this function is enough
     * to force the linker to include the DLL and trigger static initializers.
     *
     * @return Number of components registered by this package
     */
    DEKI_LOVYANGFX_API int DekiLovyanGFXEnsureRegistered(void)
    {
        if (s_LovyanGFXRegistered)
        {
            return ::DekiLovyanGFXGetAutoComponentCount();
        }
        s_LovyanGFXRegistered = true;

        // Auto-generated: registers all LovyanGFX components with ComponentRegistry + ComponentFactory
        ::DekiLovyanGFXRegisterComponents();

        return ::DekiLovyanGFXGetAutoComponentCount();
    }

    // =============================================================================
    // Plugin metadata (for dynamic loading compatibility)
    // =============================================================================

    DEKI_PLUGIN_API const char* DekiPluginGetName(void)
    {
        return "Deki LovyanGFX Package";
    }

    DEKI_PLUGIN_API const char* DekiPluginGetVersion(void)
    {
#ifdef DEKI_PACKAGE_VERSION
        return DEKI_PACKAGE_VERSION;
#else
        return "0.0.0-dev";
#endif
    }

    DEKI_PLUGIN_API int DekiPluginInit(void)
    {
        return 0;
    }

    DEKI_PLUGIN_API void DekiPluginShutdown(void)
    {
        s_LovyanGFXRegistered = false;
    }

    DEKI_PLUGIN_API int DekiPluginGetComponentCount(void)
    {
        return ::DekiLovyanGFXGetAutoComponentCount();
    }

    DEKI_PLUGIN_API const Deki::ComponentMeta* DekiPluginGetComponentMeta(int index)
    {
        return ::DekiLovyanGFXGetAutoComponentMeta(index);
    }

    DEKI_PLUGIN_API void DekiPluginRegisterComponents(void)
    {
        DekiLovyanGFXEnsureRegistered();
    }

    // =============================================================================
    // Package-specific feature API (for linked DLL access without name conflicts)
    // =============================================================================

    DEKI_LOVYANGFX_API const char* DekiLovyanGFXGetName(void)
    {
        return "LovyanGFX";
    }

}  // extern "C"

#else  // !DEKI_EDITOR - Runtime (ESP32) registration

// For runtime builds, component registration happens via static initializers
// or explicit calls from the application

#endif  // DEKI_EDITOR
}  // namespace DekiLovyanGfx
