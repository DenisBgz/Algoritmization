#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>

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

void quickSort(vector<int>& arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

void parallelQuickSort(vector<int>& arr, int low, int high, int threads) {
    if (threads <= 1 || high - low < 1000) {
        quickSort(arr, low, high);
        return;
    }

    if (low < high) {
        int pi = partition(arr, low, high);
        thread leftThread(parallelQuickSort, ref(arr), low, pi - 1, threads / 2);
        thread rightThread(parallelQuickSort, ref(arr), pi + 1, high, threads / 2);
        leftThread.join();
        rightThread.join();
    }
}

vector<int> generateArray(int size) {
    vector<int> arr(size);
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 100000;
    }
    return arr;
}

int main() {
    setlocale(LC_ALL, "RU");
    cout << fixed << setprecision(6);

    vector<int> sizes = { 100, 1000, 10000, 20000, 30000, 40000, 50000 };
    cout << "Размер\t" << "Обычная\t" << "\t" << "2 потока\t" << "4 потока\t" << "8 потоков" << endl;

    for (int size : sizes) {
        double normalTime = 0, time2 = 0, time4 = 0, time8 = 0;

        for (int k = 0; k < 100; k++) {
            vector<int> base = generateArray(size);

            vector<int> arr1 = base;
            auto start = high_resolution_clock::now();
            quickSort(arr1, 0, arr1.size() - 1);
            auto end = high_resolution_clock::now();
            normalTime += duration<double>(end - start).count();

            vector<int> arr2 = base;
            start = high_resolution_clock::now();
            parallelQuickSort(arr2, 0, arr2.size() - 1, 2);
            end = high_resolution_clock::now();
            time2 += duration<double>(end - start).count();

            vector<int> arr4 = base;
            start = high_resolution_clock::now();
            parallelQuickSort(arr4, 0, arr4.size() - 1, 4);
            end = high_resolution_clock::now();
            time4 += duration<double>(end - start).count();

            vector<int> arr8 = base;
            start = high_resolution_clock::now();
            parallelQuickSort(arr8, 0, arr8.size() - 1, 8);
            end = high_resolution_clock::now();
            time8 += duration<double>(end - start).count();
        }

        normalTime /= 100.0;
        time2 /= 100.0;
        time4 /= 100.0;
        time8 /= 100.0;

        cout << size << "\t" << normalTime << "\t" << time2 << "\t" << time4 << "\t" << time8 << endl;
    }

    return 0;
}