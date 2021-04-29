# create "datafiles" folder
if [ ! -d "datafiles/" ]
then
    mkdir "datafiles/"
fi

set -e

COMMON_ARGS="-std=gnu++17 peggml.cpp -static-libgcc -static-libstdc++ -pthread"

g++      $COMMON_ARGS  -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.so.64 -s
cp datafiles/libpeggml.so.64 datafiles/libpeggml.so
g++ -m32 $COMMON_ARGS  -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.so.32 -s
# g++ $COMMON_ARGS -fPIC -o peggml_test -g

# emscripten
if command -v emcc
then
    emcc $COMMON_ARGS -s ASYNCIFY -o datafiles/libpeggml.js  -DPEGGML_ISL_DLL -o datafiles/libpeggml.js -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
fi