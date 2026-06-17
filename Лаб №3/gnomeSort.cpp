#include <iostream>
#include <vector>
#include <chrono>
using namespace std;
using namespace chrono;


void gnomeSort(vector<int>& massive) {
    int i = 1;
    int n = massive.size();
    while (i < n) {
        if (i > 0 && massive[i - 1] > massive[i]) {
            int temp = massive[i - 1];
            massive[i - 1] = massive[i];
            massive[i] = temp;
            i--;
        }
        else {
            i++;
        }
    }
}

vector<int> generatearray(int size) {
    vector<int> arr(size);
    for (int i = 0; i < size; i++)
        arr[i] = rand() % 100000;
    return arr;
}

int main() {
    vector<int> arr = { 4, 2, 6, 7, 5, 2, 6, 9 };
    gnomeSort(arr); 
    
    cout << "Sorted demo array: ";
    for (int x : arr) cout << x << " ";
    cout << endl;

    vector<int> sizes = { 1000, 5000, 10000, 20000 };
    
    cout << "\nSize\tTime (ms)" << endl;
    cout << "------------------" << endl;

    for (int size : sizes) {
        vector<int> testArr = generatearray(size);
        
        auto start = high_resolution_clock::now();
        gnomeSort(testArr);
        auto end = high_resolution_clock::now();
        
        auto time = duration_cast<milliseconds>(end - start).count();

        cout << size << "\t" << time << " ms" << endl;
    }

    return 0;
}


