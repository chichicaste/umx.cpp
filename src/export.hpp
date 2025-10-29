#ifndef EXPORT_HPP
#define EXPORT_HPP

// Define export/import macros for Windows DLL
#ifdef _WIN32
    #ifdef umx_cpp_lib_EXPORTS
        #define UMX_API __declspec(dllexport)
    #else
        #define UMX_API __declspec(dllimport)
    #endif
#else
    #define UMX_API
#endif

#endif // EXPORT_HPP