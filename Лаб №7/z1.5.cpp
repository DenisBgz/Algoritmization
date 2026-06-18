#include <iostream>

using namespace std;

struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
};

class Solution {
public:
    long long dfs(TreeNode* node, long long currentSum) {
        if (!node) return 0;

        currentSum = currentSum * 10 + node->val;

        if (!node->left && !node->right) {
            return currentSum;
        }

        return dfs(node->left, currentSum) + dfs(node->right, currentSum);
    }

    long long sumNumbers(TreeNode* root) {
        return dfs(root, 0);
    }
};

int main() {
    TreeNode* root = new TreeNode(1);
    root->left = new TreeNode(2);
    root->right = new TreeNode(3);

    Solution sol;
    cout << "Итоговая сумма: " << sol.sumNumbers(root) << endl;

    delete root->left;
    delete root->right;
    delete root;

    return 0;
}