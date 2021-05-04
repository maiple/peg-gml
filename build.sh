# create "datafiles" folder
if [ ! -d "datafiles/" ]
then
    mkdir "datafiles/"
fi

set -e

OPTIMIZATIONS="
-fauto-inc-dec 
-fbranch-count-reg 
-fcombine-stack-adjustments 
-fcompare-elim 
-fcprop-registers 
-fdce 
-fdefer-pop 
-fdse 
-fforward-propagate 
-fguess-branch-probability 
-fif-conversion 
-fif-conversion2 
-finline-functions-called-once 
-fipa-profile 
-fipa-pure-const 
-fipa-reference 
-fipa-reference-addressable 
-fmerge-constants 
-fmove-loop-invariants 
-fomit-frame-pointer 
-freorder-blocks 
-fshrink-wrap 
-fshrink-wrap-separate 
-fsplit-wide-types 
-fssa-backprop 
-fssa-phiopt 
-ftree-bit-ccp 
-ftree-ccp 
-ftree-ch 
-ftree-coalesce-vars 
-ftree-copy-prop 
-ftree-dce 
-ftree-dominator-opts 
-ftree-dse 
-ftree-forwprop 
-ftree-fre 
-ftree-phiprop 
-ftree-pta 
-ftree-scev-cprop 
-ftree-sink 
-ftree-slsr 
-ftree-sra 
-ftree-ter 
-fno-unit-at-a-time

-falign-functions  -falign-jumps 
-falign-labels  -falign-loops 
-fcaller-saves 
-fcode-hoisting 
-fcrossjumping 
-fcse-follow-jumps  -fcse-skip-blocks 
-fdelete-null-pointer-checks 
-fdevirtualize  -fdevirtualize-speculatively 
-fexpensive-optimizations 
-fgcse  -fgcse-lm  
-fhoist-adjacent-loads 
-finline-small-functions 
-findirect-inlining 
-fipa-bit-cp  -fipa-cp  -fipa-icf 
-fipa-ra  -fipa-sra  -fipa-vrp 
-fisolate-erroneous-paths-dereference 
-flra-remat 
-foptimize-sibling-calls 
-foptimize-strlen 
-fpartial-inlining 
-fpeephole2 
-freorder-blocks-algorithm=stc 
-freorder-functions 
-frerun-cse-after-loop  
-fschedule-insns  -fschedule-insns2 
-fsched-interblock  -fsched-spec 
-fstore-merging 
-fstrict-aliasing 
-fthread-jumps 
-ftree-builtin-call-dce 
-ftree-pre 
-ftree-switch-conversion  -ftree-tail-merge 
-ftree-vrp
-fgcse-after-reload 
-finline-functions 
-fipa-cp-clone
-floop-interchange 
-floop-unroll-and-jam 
-fpeel-loops 
-fpredictive-commoning 
-fsplit-paths 
-ftree-loop-distribute-patterns 
-ftree-loop-distribution 
-ftree-loop-vectorize 
-ftree-partial-pre 
-ftree-slp-vectorize 
-funswitch-loops 
-fvect-cost-model 
-fversion-loops-for-strides
"

COMMON_ARGS="-std=gnu++17 callstack.cpp $OPTIMIZATIONS peggml.cpp -static-libgcc -static-libstdc++ -pthread -Wl,-Bstatic -lpthread -Wl,-Bdynamic"

# build linux
if command -v g++ && [ "$PEGGML_BUILD_GCC" != "0" ]
then
    # test executables
    echo "building for linux... (64-bit test)"
    g++ $COMMON_ARGS -m64 -fPIC -o peggml_test.64 -g
    echo "running... (64-bit test)"
    ./peggml_test.64
    echo "building for linux... (32-bit test)"
    g++ $COMMON_ARGS -m32 -fPIC -o peggml_test.32 -g
    echo "running... (32-bit test)"
    ./peggml_test.32

    # libraries
    echo "building for linux... (64-bit library)"
    g++ -m64 $COMMON_ARGS  -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.so.64 -s
    cp datafiles/libpeggml.so.64 datafiles/libpeggml.so
    echo "building for linux... (32-bit library)"
    g++ -m32 $COMMON_ARGS  -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.so.32 -s
fi

# build windows
if command -v x86_64-w64-mingw32-g++ && [ "$PEGGML_BUILD_MINGW" != "0" ]
then
    # `apt install mingw-w64` should suffice
    MINGW64="x86_64-w64-mingw32-g++-posix -m64"
    MINGW32="i686-w64-mingw32-g++-posix -m32"
    WINARGS="-D_WIN32_WINNT=0x0601" # target windows 7
    echo "building for windows... (64-bit library)"
    $MINGW64 $COMMON_ARGS $WINARGS -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.dll.64 -s
    echo "building for windows... (32-bit library)"
    $MINGW32 $COMMON_ARGS $WINARGS -shared  -fPIC -DPEGGML_IS_DLL -o datafiles/libpeggml.dll.32 -s
    cp datafiles/libpeggml.dll.32 datafiles/libpeggml.dll
    echo "building for windows... (64-bit test)"
    $MINGW64 $COMMON_ARGS $WINARGS -fPIC -o peggml_test.64.exe -g
    echo "building for windows... (32-bit test)"
    $MINGW32 $COMMON_ARGS $WINARGS -fPIC -o peggml_test.32.exe -g
fi

# emscripten
if command -v emcc && [ "$PEGGML_BUILD_EMCC" != "0" ]
then
    echo "building for emscripten..."
    emcc -std=gnu++17 peggml.cpp -pthread -s ASYNCIFY -o datafiles/libpeggml.js  -DPEGGML_ISL_DLL -o datafiles/libpeggml.js -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
fi