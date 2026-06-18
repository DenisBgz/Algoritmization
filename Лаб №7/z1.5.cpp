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
    long long dfs(TreeNode* node, long long current) {
        if (!node) return 0;
        current = current * 10 + node->val;

        if (!node->left && !node->right) {
            return current;
        }
        long long sum = 0;
        if (node->left)  sum += dfs(node->left, current);
        if (node->right) sum += dfs(node->right, current);
        return sum;
    }
    long long sumNumbers(TreeNode* root) {
        return dfs(root, 0);
    }
};