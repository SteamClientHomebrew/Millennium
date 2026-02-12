#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#include <iostream>
#include "exec.h"

pid_t StartMillennium() 
{
    @autoreleasepool 
    {
        NSString *currentDirectoryPath = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
        NSString *executablePath = [currentDirectoryPath stringByAppendingPathComponent:@"Millennium.Patcher"];
        
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:executablePath]) 
        {
            std::cout << "Error: Millennium executable not found at: " << executablePath.UTF8String << std::endl;
            return -1;
        }
        
        if (![fileManager isExecutableFileAtPath:executablePath]) 
        {
            std::cout << "Error: File is not executable: " << executablePath.UTF8String << std::endl;
            return -1;
        }
        
        // Set up log file at /tmp/millennium.out
        NSString *logFilePath = @"/tmp/millennium.out";
        [fileManager createFileAtPath:logFilePath contents:nil attributes:nil];
        NSFileHandle *logFileHandle = [NSFileHandle fileHandleForWritingAtPath:logFilePath];
        [logFileHandle truncateFileAtOffset:0]; // Overwrite instead of append

        NSTask *task = [[NSTask alloc] init];
        task.launchPath = executablePath;
        task.currentDirectoryPath = currentDirectoryPath;
        task.environment = [[NSProcessInfo processInfo] environment];
        task.standardOutput = logFileHandle;
        task.standardError = logFileHandle;

        task.terminationHandler = ^(NSTask *task) {
            if ([task terminationStatus] != 0) {
                std::cout << "Millennium process (PID: " << [task processIdentifier] << ") terminated with error code: " << [task terminationStatus] << std::endl;
            }
            [logFileHandle closeFile];
        };
        
        @try 
        {
            [task launch];
            pid_t pid = [task processIdentifier];
            std::cout << "Successfully launched Millennium with PID: " << pid << std::endl;
            return pid;
        } 
        @catch (NSException *exception) 
        {
            std::cout << "Failed to launch Millennium: " << exception.reason.UTF8String << std::endl;
            [logFileHandle closeFile];
            return -1;
        }
    }
}
