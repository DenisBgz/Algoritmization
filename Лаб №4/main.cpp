#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std;
using namespace chrono;

// Разбиение с выбором среднего элемента как pivot
int partition(vector<int>& arr, int left, int right) {
    int pivot = arr[left + (right - left) / 2];
    int i = left;
    int j = right;

    while (i <= j) {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        if (i <= j) {
            swap(arr[i], arr[j]);
            i++;
            j--;
        }
    }
    return i;
}

// Последовательная быстрая сортировка
void quickSortSequential(vector<int>& arr, int left, int right) {
    if (left >= right) return;

    int index = partition(arr, left, right);
    quickSortSequential(arr, left, index - 1);
    quickSortSequential(arr, index, right);
}

// Параллельная версия через std::async
void quickSortParallel(vector<int>& arr, int left, int right, int depth) {
    if (left >= right) return;

    // Если глубина достигнута — продолжаем последовательно
    if (depth <= 0 || right - left < 1000) {
        quickSortSequential(arr, left, right);
        return;
    }

    int index = partition(arr, left, right);

    auto futureLeft = async(launch::async, quickSortParallel,
                            ref(arr), left, index - 1, depth - 1);

    quickSortParallel(arr, index, right, depth - 1);

    futureLeft.wait();
}

// Генерация случайного массива
vector<int> createRandomArray(int size) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(0, 100000);

    vector<int> data(size);
    for (int& x : data)
        x = dist(gen);

    return data;
}

int main() {
    setlocale(LC_ALL, "RU");
    cout << fixed << setprecision(6);

    vector<int> sizes = {100, 1000, 10000, 20000, 30000, 40000, 50000};

    cout << "Размер\tОбычная\t\t2 потока\t4 потока\t8 потоков\n";

    for (int size : sizes) {

        double tSeq = 0, t2 = 0, t4 = 0, t8 = 0;

        for (int i = 0; i < 100; ++i) {

            vector<int> base = createRandomArray(size);

            // Обычная
            auto arr1 = base;
            auto start = high_resolution_clock::now();
            quickSortSequential(arr1, 0, arr1.size() - 1);
            auto end = high_resolution_clock::now();
            tSeq += duration<double>(end - start).count();

            // 2 потока (глубина = 1)
            auto arr2 = base;
            start = high_resolution_clock::now();
            quickSortParallel(arr2, 0, arr2.size() - 1, 1);
            end = high_resolution_clock::now();
            t2 += duration<double>(end - start).count();

            // 4 потока (глубина = 2)
            auto arr4 = base;
            start = high_resolution_clock::now();
            quickSortParallel(arr4, 0, arr4.size() - 1, 2);
            end = high_resolution_clock::now();
            t4 += duration<double>(end - start).count();

            // 8 потоков (глубина = 3)
            auto arr8 = base;
            start = high_resolution_clock::now();
            quickSortParallel(arr8, 0, arr8.size() - 1, 3);
            end = high_resolution_clock::now();
            t8 += duration<double>(end - start).count();
        }

        tSeq /= 100.0;
        t2 /= 100.0;
        t4 /= 100.0;
        t8 /= 100.0;

        cout << size << "\t"
             << tSeq << "\t"
             << t2 << "\t"
             << t4 << "\t"
             << t8 << endl;
    }

    return 0;
}