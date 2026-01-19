Compilar en el server

mkdir build && cd build
cmake ..
make
./server





Compilar en el cliente

mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/ruta/a/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .