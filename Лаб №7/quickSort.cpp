#include <iostream>
#include <vector>
#include <chrono>
using namespace std;
using namespace chrono;


int partition(vector<int>& arr, int low, int high) {
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(arr[i], arr[j]);
        }
    }

    swap(arr[i + 1], arr[high]);
    return i + 1;
}

void quicksort(vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);

        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

vector<int> generatearray(int size) {
    vector<int> arr(size);
    for (int i = 0; i < size; i++)
        arr[i] = rand() % 100000;
    return arr;
}
int main(){
    vector<int> arr = { 4, 2, 6, 7, 5, 2, 6, 9 };
    quicksort(arr, 0, arr.size() - 1);
    for (int x : arr)
        cout << x << " ";

    vector<int> sizes = { 1000, 5000, 10000, 20000 };
    for (int size : sizes) {
        vector<int> arr = generatearray(size);
        auto start = high_resolution_clock::now();
        quicksort(arr, 0, arr.size() - 1);
        auto end = high_resolution_clock::now();
        auto time = duration_cast<microseconds>(end - start).count();

        cout << "\n" << size << " " << time;
    }

    return 0;
}



