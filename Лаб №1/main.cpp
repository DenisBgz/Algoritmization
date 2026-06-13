#include <iostream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iomanip>
using namespace std;

long long measureInsertTime(size_t n) {
    unordered_map<size_t, size_t> dict;

    for (size_t i = 0; i < n; ++i) {
        dict[i] = i;
    }

    auto start = chrono::high_resolution_clock::now();
    dict[n] = n;
    auto end = chrono::high_resolution_clock::now();

    return chrono::duration_cast<chrono::nanoseconds>(end - start).count();
}

int main() {
    vector<size_t> sizes = {
        10, 100, 1000, 10000, 100000, 1000000, 5000000, 10000000
    };
    const int repetitions = 10;

    for (size_t n : sizes) {
        long long totalTime = 0;
        for (int rep = 0; rep < repetitions; ++rep) {
            totalTime += measureInsertTime(n);
        }
        double avgTime = static_cast<double>(totalTime) / repetitions;
        cout << setw(15) << n
            << setw(20) << fixed << setprecision(2) << avgTime << endl;
    }


    cin.get(); 
    return 0;
}