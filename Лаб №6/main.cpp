#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <random>
#include <map>
#include <set>
#include <functional>
#include <climits>
#include <windows.h>

using namespace std;
using namespace sf;

// 10x10, каждая клетка — 60 пикселей, чтобы окно было 600x600
const int GRID_SIZE = 10;
const int CELL_SIZE = 60;
const int WINDOW_SIZE = GRID_SIZE * CELL_SIZE;



// Состояния клеток для отрисовки разными цветами
enum CellType {
    EMPTY,
    OBSTACLE,
    START,
    END,
    PATH,
    VISITED,
    IN_QUEUE
};

// Каждая клетка хранит свои координаты, тип, значения для A* и вес
struct Cell {
    int x, y;
    CellType type;
    int f, g, h;
    int weight;
    Cell* parent;

    Cell(int x = 0, int y = 0)
        : x(x), y(y), type(EMPTY), f(0), g(0), h(0), weight(1), parent(nullptr) {
    }

    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y;
    }
};

// Заранее заданная карта препятствий (как на рисунке)
// Строки — это Y, столбцы — X, так что FIXED_GRID[y][x]
int FIXED_GRID[10][10] = {
    {0,0,0,1,0,1,0,0,0,0},
    {0,1,1,0,1,0,0,0,0,1},
    {0,0,0,0,0,0,0,1,0,0},
    {1,0,0,1,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1,0,0},
    {0,0,0,0,0,0,0,1,1,0},
    {0,0,0,0,1,1,0,0,0,0},
    {0,1,0,0,0,0,0,1,0,0},
    {0,0,1,0,0,1,0,0,0,0},
    {0,0,1,1,0,1,1,0,1,0}
};

// Манхэттенское расстояние — простая и допустимая эвристика
int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

bool isValid(int x, int y) {
    return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE;
}

// Цвета для визуализации разных типов клеток
Color getCellColor(const Cell& cell) {
    switch (cell.type) {
    case EMPTY:    return Color::White;
    case OBSTACLE: return Color::Black;
    case START:    return Color(255, 165, 0);
    case END:      return Color(0, 200, 200);
    case PATH:     return Color(128, 0, 128);
    case VISITED:  return Color(255, 100, 100);
    case IN_QUEUE: return Color(100, 255, 100);
    default:       return Color::White;
    }
}

// Для режима карты с весами: чем выше вес, тем краснее
Color getWeightColor(int weight) {
    int r = min(255, (weight - 1) * 30);
    int g = max(0, 200 - (weight - 1) * 20);
    return Color(r, g, 80);
}

// Стираем только путь и пометки поиска, оставляя препятствия
void clearPath(vector<vector<Cell>>& grid) {
    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            Cell& c = grid[x][y];
            if (c.type == PATH || c.type == VISITED || c.type == IN_QUEUE) {
                c.type = EMPTY;
            }
            c.f = c.g = c.h = 0;
            c.parent = nullptr;
        }
}

// Создаём сетку на основе фиксированной карты
vector<vector<Cell>> makeFixedGrid() {
    vector<vector<Cell>> grid(GRID_SIZE, vector<Cell>(GRID_SIZE));
    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            grid[x][y] = Cell(x, y);
            grid[x][y].weight = 1;
            if (FIXED_GRID[y][x] == 1)
                grid[x][y].type = OBSTACLE;
        }
    grid[0][0].type = START;
    grid[9][9].type = END;
    return grid;
}

// Случайное поле: препятствия с заданной вероятностью, но старт и финиш свободны
vector<vector<Cell>> makeRandomGrid(float obstacleProb = 0.25f) {
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> dist(0.f, 1.f);

    vector<vector<Cell>> grid(GRID_SIZE, vector<Cell>(GRID_SIZE));
    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            grid[x][y] = Cell(x, y);
            grid[x][y].weight = 1;
            if (dist(gen) < obstacleProb)
                grid[x][y].type = OBSTACLE;
        }
    grid[0][0].type = START;
    grid[GRID_SIZE - 1][GRID_SIZE - 1].type = END;
    return grid;
}

// Случайная карта с весами: каждой клетке даётся вес от 1 до 9, а также могут быть препятствия
vector<vector<Cell>> makeRandomMap() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> weightDist(1, 9);
    uniform_real_distribution<float> obsDist(0.f, 1.f);

    vector<vector<Cell>> grid(GRID_SIZE, vector<Cell>(GRID_SIZE));
    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            grid[x][y] = Cell(x, y);
            if (obsDist(gen) < 0.20f) {
                grid[x][y].type = OBSTACLE;
                grid[x][y].weight = 0;
            }
            else {
                grid[x][y].type = EMPTY;  // Явно указываем тип
                grid[x][y].weight = weightDist(gen);
            }
        }
    grid[0][0].type = START;
    grid[0][0].weight = 1;
    grid[GRID_SIZE - 1][GRID_SIZE - 1].type = END;
    grid[GRID_SIZE - 1][GRID_SIZE - 1].weight = 1;
    return grid;
}

// Сам алгоритм A*: если useWeights == true, стоимость перехода равна весу клетки
bool a_star_algorithm(vector<vector<Cell>>& grid, bool useWeights = false) {
    Cell* startCell = nullptr;
    Cell* endCell = nullptr;
    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            if (grid[x][y].type == START) startCell = &grid[x][y];
            if (grid[x][y].type == END)   endCell = &grid[x][y];
        }
    if (!startCell || !endCell) return false;

    for (int x = 0; x < GRID_SIZE; ++x)
        for (int y = 0; y < GRID_SIZE; ++y) {
            grid[x][y].g = INT_MAX;
            grid[x][y].f = INT_MAX;
            grid[x][y].parent = nullptr;
        }

    auto cmp = [](Cell* a, Cell* b) { return a->f > b->f; };
    priority_queue<Cell*, vector<Cell*>, decltype(cmp)> openQueue(cmp);

    set<pair<int, int>> openSet;
    set<pair<int, int>> closedSet;

    startCell->g = 0;
    startCell->h = heuristic(startCell->x, startCell->y, endCell->x, endCell->y);
    startCell->f = startCell->g + startCell->h;
    openQueue.push(startCell);
    openSet.insert({ startCell->x, startCell->y });

    int dx[] = { 0, 0, -1, 1 };
    int dy[] = { -1, 1, 0, 0 };

    while (!openQueue.empty()) {
        Cell* current = openQueue.top();
        openQueue.pop();
        openSet.erase({ current->x, current->y });

        if (current->x == endCell->x && current->y == endCell->y) {
            Cell* node = current->parent;
            while (node && node->type != START) {
                node->type = PATH;
                node = node->parent;
            }
            return true;
        }

        closedSet.insert({ current->x, current->y });
        if (current->type != START && current->type != END)
            current->type = VISITED;

        for (int d = 0; d < 4; ++d) {
            int nx = current->x + dx[d];
            int ny = current->y + dy[d];

            if (!isValid(nx, ny)) continue;
            Cell* neighbor = &grid[nx][ny];
            if (neighbor->type == OBSTACLE) continue;
            if (closedSet.count({ nx, ny })) continue;

            int moveCost = useWeights ? neighbor->weight : 1;
            int tentative_g = current->g + moveCost;

            if (tentative_g < neighbor->g) {
                neighbor->parent = current;
                neighbor->g = tentative_g;
                neighbor->h = heuristic(nx, ny, endCell->x, endCell->y);
                neighbor->f = neighbor->g + neighbor->h;

                if (!openSet.count({ nx, ny })) {
                    openQueue.push(neighbor);
                    openSet.insert({ nx, ny });
                    if (neighbor->type != END)
                        neighbor->type = IN_QUEUE;
                }
            }
        }
    }
    return false;
}
// Рисуем всю сетку, при режиме карты показываем цвета весов и числа
void drawGrid(RenderWindow& window, const vector<vector<Cell>>& grid,
    bool mapMode, Font& font) {
    for (int x = 0; x < GRID_SIZE; ++x) {
        for (int y = 0; y < GRID_SIZE; ++y) {
            const Cell& cell = grid[x][y];

            RectangleShape rect(Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
            rect.setPosition(x * CELL_SIZE, y * CELL_SIZE);

            if (mapMode && cell.type == EMPTY) {
                rect.setFillColor(getWeightColor(cell.weight));
            }
            else {
                rect.setFillColor(getCellColor(cell));
            }
            window.draw(rect);

            if (mapMode && cell.type == EMPTY && cell.weight > 1) {
                Text t;
                t.setFont(font);
                t.setCharacterSize(14);
                t.setFillColor(Color::White);
                t.setString(to_string(cell.weight));
                t.setPosition(x * CELL_SIZE + 4, y * CELL_SIZE + 4);
                window.draw(t);
            }
        }
    }

    for (int i = 0; i <= GRID_SIZE; ++i) {
        RectangleShape vLine(Vector2f(1, WINDOW_SIZE));
        vLine.setPosition(i * CELL_SIZE, 0);
        vLine.setFillColor(Color(180, 180, 180));
        window.draw(vLine);
        RectangleShape hLine(Vector2f(WINDOW_SIZE, 1));
        hLine.setPosition(0, i * CELL_SIZE);
        hLine.setFillColor(Color(180, 180, 180));
        window.draw(hLine);
    }
}

// Легенда для карт
void drawLegend(RenderWindow& window, Font& font, bool mapMode)
{
    struct LegendItem {
        Color color;
        string label;
    };

    vector<LegendItem> items = {
        {Color(255,165,0),   "Start"},
        {Color(0,200,200),   "Goal"},
        {Color(128,0,128),   "Path"},
        {Color(255,100,100), "Visited"},
        {Color(100,255,100), "Open List"},
        {Color::Black,       "Obstacle"}
    };

    // Дополнительные элементы только для карты весов
    if (mapMode)
    {
        items.push_back({ Color(80,200,80),  "Weight 1" });
        items.push_back({ Color(200,80,80),  "Weight 9" });
    }

    int startX = WINDOW_SIZE + 10;
    int y = 10;

    for (const auto& item : items)
    {
        RectangleShape rect(Vector2f(20, 20));
        rect.setPosition(startX, y);
        rect.setFillColor(item.color);
        rect.setOutlineThickness(1);
        rect.setOutlineColor(Color::White);

        window.draw(rect);

        Text text;
        text.setFont(font);
        text.setCharacterSize(14);
        text.setFillColor(Color::White);
        text.setString(item.label);
        text.setPosition(startX + 30, y + 1);

        window.draw(text);

        y += 30;
    }
}

// Точка входа: создаём окно, обрабатываем клавиши,запускаем A* и отображаем всё
int main() {

    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    int mode = 0;

    vector<vector<Cell>> grid = makeFixedGrid();
    bool mapMode = false;
    bool pathFound = false;
    bool pathSearched = false;

    RenderWindow window(VideoMode(WINDOW_SIZE + 160, WINDOW_SIZE),
        "A* Pathfinding - C++ SFML");
    window.setFramerateLimit(60);

    Font font;
    bool fontLoaded = font.loadFromFile("arial.ttf");

    cout << "=== A* Pathfinding ===" << endl;
    cout << "SPACE  - запустить A*" << endl;
    cout << "R      - случайное поле" << endl;
    cout << "M      - карта с весами" << endl;
    cout << "F      - фиксированная сетка" << endl;
    cout << "C      - очистить путь" << endl;

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed)
                window.close();

            if (event.type == Event::KeyPressed) {
                switch (event.key.code) {
                case Keyboard::Space:
                    clearPath(grid);
                    pathFound = a_star_algorithm(grid, mapMode);
                    pathSearched = true;
                    if (!pathFound)
                        cout << "[A*] Путь не найден!" << endl;
                    else
                        cout << "[A*] Путь найден!" << endl;
                    break;

                case Keyboard::R:
                    grid = makeRandomGrid();
                    mapMode = false;
                    pathFound = false;
                    pathSearched = false;
                    mode = 1;
                    cout << "[R] Новое случайное поле" << endl;
                    break;

                case Keyboard::M:
                    grid = makeRandomMap();
                    mapMode = true;
                    pathFound = false;
                    pathSearched = false;
                    mode = 2;
                    cout << "[M] Новая карта с весами" << endl;
                    break;

                case Keyboard::F:
                    grid = makeFixedGrid();
                    mapMode = false;
                    pathFound = false;
                    pathSearched = false;
                    mode = 0;
                    cout << "[F] Фиксированная сетка" << endl;
                    break;

                case Keyboard::C:
                    clearPath(grid);
                    pathFound = false;
                    pathSearched = false;
                    break;

                default: break;
                }
            }
        }

        window.clear(Color(40, 40, 40));

        drawGrid(window, grid, mapMode, font);

        if (fontLoaded)
            drawLegend(window, font, mapMode);

        if (fontLoaded) {
            string hint = "SPACE=A*  R=Поле  M=Карта  F=Фикс  C=Очистить";
            if (pathSearched)
                hint = pathFound ? "[Путь найден!] " + hint : "[Нет пути!] " + hint;
            Text t;
            t.setFont(font);
            t.setCharacterSize(11);
            t.setFillColor(Color(200, 200, 200));
            t.setString(hint);
            t.setPosition(4, WINDOW_SIZE - 18);
            RectangleShape bg(Vector2f(WINDOW_SIZE, 20));
            bg.setPosition(0, WINDOW_SIZE - 20);
            bg.setFillColor(Color(20, 20, 20, 200));
            window.draw(bg);
            window.draw(t);
        }

        window.display();
    }

    return 0;
}