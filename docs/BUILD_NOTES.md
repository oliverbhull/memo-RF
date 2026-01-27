# Build Notes

## whisper.cpp Setup

Your whisper.cpp is located at: `/Users/oliverhull/dev/whisper.cpp`

### Building whisper.cpp

whisper.cpp can be built with either Make or CMake:

**Using Make:**
```bash
cd /Users/oliverhull/dev/whisper.cpp
make
```

**Using CMake:**
```bash
cd /Users/oliverhull/dev/whisper.cpp
mkdir -p build
cd build
cmake ..
make
```

### Finding the Library

After building, the library will be:
- **Make build**: Usually creates `libwhisper.a` in the root directory
- **CMake build**: Creates library in `build/` directory

The header file is at: `/Users/oliverhull/dev/whisper.cpp/include/whisper.h`

### If Build Fails to Find whisper.cpp

If CMake can't find whisper.cpp, specify the path explicitly:

```bash
cd build
cmake -DWHISPER_DIR=/Users/oliverhull/dev/whisper.cpp ..
make
```

## Model Files

Your Whisper models are in: `~/models/whisper/`

Update `config/config.json` to point to your model:
```json
{
  "stt": {
    "model_path": "/Users/oliverhull/models/whisper/ggml-small.en-q5_1.bin"
  }
}
```
