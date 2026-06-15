#include <iostream>
#include <cmath>
#include <locale>

double simpson_rule(double (*f)(double), double a, double b, int n) {
    if (n % 2 == 1) n++;
    double h = (b - a) / n;
    double sum = f(a) + f(b);

    for (int i = 1; i < n; i += 2)
        sum += 4 * f(a + i * h);

    for (int i = 2; i < n - 1; i += 2)
        sum += 2 * f(a + i * h);

    return (h / 3) * sum;
}

double function(double x) {
    return 2 * x * x + 8;
}

int main() {
    setlocale(LC_ALL, "Russian");
    double a = 3.0;
    double b = 4.0;
    int n = 10000;

    double result = simpson_rule(function, a, b, n);
    std::cout << "Примерная площадь: " << result << std::endl;
    return 0;
}