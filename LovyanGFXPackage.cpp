// Entry point of the deki-lovyangfx DLL. Exports the standard Deki plugin
// interface, so the editor can load it and find its LovyanGFX components.
//
// When the DLL is linked rather than loaded at run time, the main executable
// must call DekiLovyanGFXEnsureRegistered() to run the static initialisers.

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

// Set once registered, so registration never runs twice.
static bool s_LovyanGFXRegistered = false;

// The exports below are C symbols at global scope; the package's own
// registration helpers and statics live in its namespace.
using namespace DekiLovyanGfx;

extern "C"
{
    /// Makes sure the package is loaded and its components registered, and
    /// returns how many components it registered. Call it from the editor at
    /// startup; the call alone makes the linker keep the DLL and run its
    /// static initialisers.
    DEKI_LOVYANGFX_API int DekiLovyanGFXEnsureRegistered(void)
    {
        if (s_LovyanGFXRegistered)
        {
            return ::DekiLovyanGFXGetAutoComponentCount();
        }
        s_LovyanGFXRegistered = true;

        // Generated: registers every LovyanGFX component with ComponentRegistry and ComponentFactory.
        ::DekiLovyanGFXRegisterComponents();

        return ::DekiLovyanGFXGetAutoComponentCount();
    }

    // =============================================================================
    // Plugin metadata, for dynamic loading
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
    // Package-specific API, named so linked DLLs do not clash
    // =============================================================================

    DEKI_LOVYANGFX_API const char* DekiLovyanGFXGetName(void)
    {
        return "LovyanGFX";
    }

}  // extern "C"

#else  // !DEKI_EDITOR: runtime (ESP32) registration

// In runtime builds, components are registered by static initialisers or by
// explicit calls from the application.

#endif  // DEKI_EDITOR
}  // namespace DekiLovyanGfx
