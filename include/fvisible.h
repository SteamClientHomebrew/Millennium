#ifndef MILLENNIUM_EXPORT_H
#define MILLENNIUM_EXPORT_H

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