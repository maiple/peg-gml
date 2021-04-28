# create "datafiles" folder
if [ ! -d "datafiles/" ]
then
    mkdir "datafiles/"
fi

COMMON_ARGS="-std=gnu++17 peggml.cpp -static-libgcc -static-libstdc++ -pthread"

# g++ -m32 $COMMON_ARGS  -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.so -s
#g++ $COMMON_ARGS -fPIC -o peggml_test -g

# emscripten
if command -v emcc
then
    emcc $COMMON_ARGS -s ASYNCIFY
fi