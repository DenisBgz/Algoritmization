#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
using namespace std;

long long parseTime(const string& s) {
    size_t dot = s.find('.');
    if (dot == string::npos) {
        return stoll(s) * 60;
    }
    long long h = stoll(s.substr(0, dot));
    long long m = stoll(s.substr(dot + 1));
    return h * 60 + m;
}

string formatTime(long long t) {
    long long h = t / 60, m = t % 60;
    if (m == 0) return to_string(h);
    return to_string(h) + "." + to_string(m);
}

struct Lesson {
    long long start, end;
    int idx;
};

int main() {
    int n;
    cin >> n;
    vector<Lesson> lessons(n);

    for (int i = 0; i < n; i++) {
        string a, b;
        cin >> a >> b;
        lessons[i] = { parseTime(a), parseTime(b), i };
    }

    sort(lessons.begin(), lessons.end(), [](const Lesson& a, const Lesson& b) {
        return a.end < b.end;
    });

    vector<Lesson> chosen;
    long long lastEnd = -1;
    for (const auto& l : lessons) {
        if (l.start >= lastEnd) {
            chosen.push_back(l);
            lastEnd = l.end;
        }
    }

    cout << chosen.size() << "\n";
    for (const auto& l : chosen) {
        cout << formatTime(l.start) << " " << formatTime(l.end) << "\n";
    }
    return 0;
}