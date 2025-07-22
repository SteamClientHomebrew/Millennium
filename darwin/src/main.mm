#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <ServiceManagement/ServiceManagement.h>
#include <libproc.h>
#include "watchdog.h"
#include "exec.h"
#include "logger.h"
#include <iostream>

pid_t g_millenniumPID = -1;

namespace EventHandler
{
    void OnSteamStart() 
    {
        LOG_INFO("Steam launched! Starting Millennium process.");
        g_millenniumPID = StartMillennium();
    }

    void OnSteamStop() 
    {
        LOG_INFO("Terminating Millennium...");

        if (g_millenniumPID != -1) 
        {
            kill(g_millenniumPID, SIGTERM);
            LOG_INFO("Terminated Millennium process with PID: %d", g_millenniumPID);
            g_millenniumPID = -1;
            return;
        }

        LOG_INFO("No Millennium process to terminate.");
    }
}

void SignalHandler(int) 
{
    EventHandler::OnSteamStop();
    LOG_INFO("Shutting down Millennium Steam sniper...");
    exit(0);
}

int main(int argc, const char * argv[])
{
    bool isLaunchd = std::any_of(argv, argv + argc, [](const char* arg) { return std::strcmp(arg, "--daemon") == 0; });
    if (!isLaunchd)
    {
        @autoreleasepool 
        {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Steam Client Homebrew"];
            [alert setInformativeText:@"No need to run Millennium directly — just launch Steam and we'll take care of the rest!"];
            [alert addButtonWithTitle:@"OK"];
            
            if ([alert runModal] == NSAlertFirstButtonReturn) 
            {
                std::exit(0);
            }
        }
    }

    LOG_INFO("Starting Millennium Steam sniper...");
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    void* watchDog = SteamWatchDog_Start(EventHandler::OnSteamStart, EventHandler::OnSteamStop);
    [[NSRunLoop mainRunLoop] run];

    SteamWatchDog_Stop(watchDog);
    EventHandler::OnSteamStop();
    return 0;
}
