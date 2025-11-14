// Platform-agnostic process implementation
// Uses conditional compilation to select platform-specific implementation

#ifdef _WIN32
    #include "process_win32.cpp"
#else
    #include "process_posix.cpp"
#endif
