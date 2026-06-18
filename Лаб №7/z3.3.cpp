#include <iostream>
#include <vector>
using namespace std;

vector<int> merge(const vector<int>& left, const vector<int>& right) {
    vector<int> result;

    for (int x : left)  if (x < 0) result.push_back(x);
    for (int x : right) if (x < 0) result.push_back(x);

    for (int x : left)  if (x >= 0) result.push_back(x);
    for (int x : right) if (x >= 0) result.push_back(x);

    return result;
}

vector<int> divideAndConquer(const vector<int>& a) {
    if (a.size() <= 1) return a;

    int mid = a.size() / 2;
    vector<int> left(a.begin(), a.begin() + mid);
    vector<int> right(a.begin() + mid, a.end());

    left = divideAndConquer(left);
    right = divideAndConquer(right);

    return merge(left, right);
}

int main() {
    int n;
    cin >> n;
    vector<int> a(n);
    for (int i = 0; i < n; i++) cin >> a[i];

    vector<int> result = divideAndConquer(a);

    for (int x : result) cout << x << " ";
    cout << "\n";
    return 0;
}