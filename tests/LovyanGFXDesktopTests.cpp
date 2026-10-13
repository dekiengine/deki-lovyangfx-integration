// deki-lovyangfx-integration off a device. The driver itself (panel set-up,
// DMA, the present task on the second core) only builds for ESP32 and is
// checked by the firmware builds; on a desktop the package compiles to stubs.
// What those stubs promise still matters: a board's boot scene has to run in
// the editor and the simulator, so the panels report success, and the stub
// display says plainly that there is no display rather than pretending.
//
// The registration functions reflection codegen generates are stood in for,
// so the plugin entry file is tested as it is.

#include <gtest/gtest.h>

#include "LGFXDisplayPanel.h"
#include "LGFXTouchPanel.h"
#include "LovyanGFXDisplay.h"

#include <deki/interop/Plugin.h>

#include <cstring>

using namespace DekiLovyanGfx;

namespace
{
int g_Registered = 0;
}

void DekiLovyanGFXRegisterComponents()
{
    ++g_Registered;
}
int DekiLovyanGFXGetAutoComponentCount()
{
    return 2;
}
const Deki::ComponentMeta* DekiLovyanGFXGetAutoComponentMeta(int)
{
    return nullptr;
}

extern "C"
{
    int DekiLovyanGFXEnsureRegistered(void);
    const char* DekiPluginGetVersion(void);
    void DekiPluginShutdown(void);
}

TEST(LovyanGFXDesktop, PanelsLetABoardsBootSceneRun)
{
    for (int i = 0; i < 2; ++i)
    {
        bool reported = false;
        bool called = false;
        auto done = [&](bool ok)
        {
            reported = ok;
            called = true;
        };
        if (i == 0)
        {
            LGFXDisplayPanel panel;
            panel.Setup(done);
        }
        else
        {
            LGFXTouchPanel panel;
            panel.Setup(done);
        }
        EXPECT_TRUE(called);
        EXPECT_TRUE(reported) << (i == 0 ? "display panel" : "touch panel");
    }
}

TEST(LovyanGFXDesktop, TheStubDisplayClaimsNothing)
{
    LovyanGFXDisplay display;
    EXPECT_FALSE(display.Initialize(320, 240));
    EXPECT_FALSE(display.IsInitialized());
    EXPECT_FALSE(display.SupportsPartialPresent());
    EXPECT_EQ(display.CreateUIOverlay(10, 10), nullptr);
    display.Present(nullptr, 0, 0, Deki::ColorFormat::RGB565);  // harmless
    display.Shutdown();
}

TEST(LovyanGFXDesktop, ComponentsAreRegisteredOnceAndAgainAfterAReload)
{
    DekiPluginShutdown();
    g_Registered = 0;
    EXPECT_EQ(DekiLovyanGFXEnsureRegistered(), 2);
    EXPECT_EQ(DekiLovyanGFXEnsureRegistered(), 2);
    EXPECT_EQ(g_Registered, 1);
    DekiPluginShutdown();
    DekiLovyanGFXEnsureRegistered();
    EXPECT_EQ(g_Registered, 2);
    EXPECT_STREQ(DekiPluginGetVersion(), "tests");
}
