# create "datafiles" folder
if [ ! -d "datafiles/" ]
then
    mkdir "datafiles/"
fi

# g++
g++ -std=gnu++17 -shared peggml.cpp -static-libgcc -static-libstdc++ -fPIC -DIS_DLL -o datafiles/libpeggml.so -s