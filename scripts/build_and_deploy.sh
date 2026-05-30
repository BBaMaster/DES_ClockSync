cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake
cmake --build build-arm
bash scripts/deploy.sh 10.0.0.11
bash scripts/deploy.sh 10.0.0.12