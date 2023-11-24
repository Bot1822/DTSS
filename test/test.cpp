#include <iostream>
#include <fstream>
#include <chrono>

int main() {
    std::ofstream file("test.log");

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++){
        file << "Some log data" << std::endl;
        file.flush();
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::micro> duration = end - start;
    std::cout << "Flush duration: " << duration.count() << " microseconds" << std::endl;

    return 0;
}
