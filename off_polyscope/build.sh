echo "Building off_polyscope..."
cmake -S . -B build
cmake --build build -j
echo "Build complete."