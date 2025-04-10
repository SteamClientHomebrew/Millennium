#ifndef MILLENNIUM_EXPORT_H
#define MILLENNIUM_EXPORT_H

/**
 * This header is used as a way to force functions to RVA map into the export table. 
 * This exposes function ordinals externally, which can be used to identify function information from non-debug stripped binaries.
 * This is useful for debugging purposes on release builds, without having debug overhead. 
 */
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef __GNUC__
        #define MILLENNIUM __attribute__((dllexport))
    #else
        #define MILLENNIUM __declspec(dllexport))
    #endif
    #define MILLENNIUM_PRIVATE
#else
    #if __GNUC__ >= 4
        #define MILLENNIUM __attribute__((visibility("default")))
        #define MILLENNIUM_PRIVATE __attribute__((visibility("hidden")))
    #else
        #define MILLENNIUM
        #define MILLENNIUM_PRIVATE
    #endif
#endif

#endif // MILLENNIUM_EXPORT_H