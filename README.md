# libmedia

libmedia based on ffmpeg

# compile

```bash
rm -rf build
cmake -S . -B build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```
