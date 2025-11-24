#!/bin/bash
set -e

echo "Очистка старой сборки..."
rm -rf build CMakeCache.txt CMakeFiles cmake_install.cmake

echo "Создание новой сборки..."
mkdir build && cd build
cmake ..
make -j$(nproc)

echo ""
echo "ГОТОВО!"
echo "Доступны три программы:"
echo "  ./build/host_mq"
echo "  ./build/host_fifo"
echo "  ./build/host_sock"
echo ""
echo "Запуск примера: ./build/host_mq"