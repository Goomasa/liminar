# Liminar

## Dependencies
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader.git)
- [tinygltf](https://github.com/syoyo/tinygltf)
- [SDL](https://github.com/libsdl-org/SDL)
- [stb](https://github.com/nothings/stb/tree/master)
- OpenMP

## Build
**MacOS**
```sh
git clone git@github.com:Goomasa/liminar.git

cmake -DCMAKE_BUILD_TYPE=Release . -B build
cmake --build build
```

**Windows (Visual Studio Generator)**
```sh
cmake . -B build 
cmake --build build --config Release
```
