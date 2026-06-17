#include <vector>
#include <algorithm>
#include <iostream>
#include <chrono>
using namespace std;
using namespace chrono;

void countingsortbydigit(vector<int>& arr, int exp) {
    int n = arr.size();
    vector<int> output(n);
    int count[10] = { 0 };

    for (int i = 0; i < n; ++i) {
        int digit = (arr[i] / exp) % 10;
        count[digit]++;
    }

    for (int i = 1; i < 10; ++i) {
        count[i] += count[i - 1];
    }

    for (int i = n - 1; i >= 0; --i) {
        int digit = (arr[i] / exp) % 10;
        output[count[digit] - 1] = arr[i];
        count[digit]--;
    }
     
    for (int i = 0; i < n; ++i) {
        arr[i] = output[i];
    }
}
void radixsort(vector<int>& arr) {
    if (arr.empty()) return;
    int maxval = *max_element(arr.begin(), arr.end());
    for (int exp = 1; maxval / exp > 0; exp *= 10) {
        countingsortbydigit(arr, exp);
    }
}


vector<int> generateArray(int size) {
    vector<int> arr(size);
    for (int i = 0; i < size; i++)
        arr[i] = rand() % 100000;
    return arr;
}
int main() {
    vector<int> data = { 4, 2, 6, 7, 5, 2, 6, 9 };
    radixsort(data);
    for (int x : data) 
	cout << x << " ";

    vector<int> sizes = { 1000, 5000, 10000, 20000 };

    for (int size : sizes) {
        vector<int> arr = generateArray(size);
        auto start = high_resolution_clock::now();
        radixsort(arr);
        auto end = high_resolution_clock::now();

        auto time = duration_cast<microseconds>(end - start).count();

        cout << "\n" << size << " " << time;
    }

    return 0;
}