# PortAudio Linker Fix

## Issue
The linker couldn't find the PortAudio library even though pkg-config found it:
```
ld: library 'portaudio' not found
```

## Solution Applied

1. **Added library directory search path** for macOS:
   ```cmake
   # Extract library directory from pkg-config
   execute_process(
       COMMAND pkg-config --libs-only-L portaudio-2.0
       OUTPUT_VARIABLE PORTAUDIO_LIB_DIRS
       OUTPUT_STRIP_TRAILING_WHITESPACE
   )
   string(REPLACE "-L" "" PORTAUDIO_LIB_DIRS "${PORTAUDIO_LIB_DIRS}")
   
   # Add to link directories
   target_link_directories(memo-rf PRIVATE ${PORTAUDIO_LIB_DIRS})
   ```

2. **Added macOS frameworks** required by PortAudio:
   ```cmake
   if(APPLE)
       find_library(COREAUDIO_FRAMEWORK CoreAudio)
       find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
       find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
       find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
       find_library(CORESERVICES_FRAMEWORK CoreServices)
       
       target_link_libraries(memo-rf
           ${COREAUDIO_FRAMEWORK}
           ${AUDIOTOOLBOX_FRAMEWORK}
           ${AUDIOUNIT_FRAMEWORK}
           ${COREFOUNDATION_FRAMEWORK}
           ${CORESERVICES_FRAMEWORK}
       )
   endif()
   ```

## Testing

Once build directory permissions are fixed, the build should complete successfully. The PortAudio library is located at:
- `/opt/homebrew/Cellar/portaudio/19.7.0/lib/libportaudio.a`
- `/opt/homebrew/Cellar/portaudio/19.7.0/lib/libportaudio.2.dylib`

## Alternative Fix (if above doesn't work)

If the linker still can't find PortAudio, you can explicitly link to the library:

```cmake
find_library(PORTAUDIO_LIB
    NAMES portaudio libportaudio
    PATHS
    /opt/homebrew/lib
    /opt/homebrew/Cellar/portaudio/*/lib
    /usr/local/lib
    NO_DEFAULT_PATH
)

if(PORTAUDIO_LIB)
    target_link_libraries(memo-rf ${PORTAUDIO_LIB})
endif()
```
