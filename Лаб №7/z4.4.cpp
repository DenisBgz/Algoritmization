#include <iostream>
#include <vector>
using namespace std;

int grid[3][3];
bool used[10];
int N;

bool isMagic() {
    int diag1 = grid[0][0] + grid[1][1] + grid[2][2];
    int diag2 = grid[0][2] + grid[1][1] + grid[2][0];
    if (diag1 != diag2) return false;

    for (int i = 0; i < 3; i++) {
        int rowSum = grid[i][0] + grid[i][1] + grid[i][2];
        int colSum = grid[0][i] + grid[1][i] + grid[2][i];
        if (rowSum != diag1 || colSum != diag1) return false;
    }
    return true;
}

bool backtrack(int pos) {
    if (pos == 9) {
        return isMagic();
    }

    int r = pos / 3, c = pos % 3;

    if (pos == 0) {
        grid[r][c] = N;
        used[N] = true;
        if (backtrack(pos + 1)) return true;
        used[N] = false;
        return false;
    }

    for (int num = 1; num <= 9; num++) {
        if (used[num]) continue;
        grid[r][c] = num;
        used[num] = true;
        if (backtrack(pos + 1)) return true;
        used[num] = false;
    }
    return false;
}

int main() {
    cin >> N;
    fill(used, used + 10, false);

    if (backtrack(0)) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                cout << grid[i][j] << " ";
            }
            cout << "\n";
        }
    } else {
        cout << -1 << "\n";
    }
    return 0;
}