#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
using namespace std;
const int START_HEALTH = 100, START_LIVES = 3, TRAP_DAMAGE = 25, MONSTER_DAMAGE = 50;
const int TREASURE_POINTS = 50, LEVEL_BONUS = 100, HINT_COST = 10, UNDO_COST = 5, LEVEL_COUNT = 3, MAX_NAME_LENGTH = 20;
const char* const SCORE_FILE = "leaderboard.txt";
const char WALL = '#', FLOOR = '.', KEY = 'K', DOOR = 'D', EXIT = 'E';
const char TREASURE = '$', TRAP = '^', HIDDEN = 'S', FOUND = ':', ROUTE = '*', CRYSTAL = 'C';
const int ROW_CHANGE[4] = {-1, 1, 0, 0}, COL_CHANGE[4] = {0, 0, -1, 1};
struct Position {
    int row, col;
    Position(int r = 0, int c = 0) { row = r; col = c; }
};
struct LevelData { int rows, monsterDelay; const char* lines[15]; };
const LevelData LEVELS[LEVEL_COUNT] = {
    { 9, 2, {
        "#####################",
        "#P....#.......#...$.#",
        "#.###.#.#####.#.###.#",
        "#...#...#.K.#...#...#",
        "###.#####.#.#####.###",
        "#...#.....#.......#E#",
        "#.###.#####.#####.#D#",
        "#.....^...$.#M......#",
        "#####################" } },
    { 11, 2, {
        "###########################",
        "#P..#.......#.....#...$...#",
        "#.#.#.#####.#.###.#.###.#.#",
        "#.#...#...#...#K#...#...#.#",
        "#.#####.#.#####.#####.###.#",
        "#.....#.#.....#.....#.#...#",
        "#####.#.#####.#.###.#.#.###",
        "#$.^..#.....#...#...#...#E#",
        "#.#########.#####.#####.#D#",
        "#..K..S.......^..D......M.#",
        "###########################" } },
    { 13, 1, {
        "###############################",
        "#P....#.....#.......#.....D.$.#",
        "#.###.#.###.#.#####.#.#####.#.#",
        "#.#...#...#...#...#...#...#.#.#",
        "#.#.#####.#####.#.#####.#.#.#.#",
        "#.#.....#.....#.#.......#.#...#",
        "#.#####.#####.#.#########.###.#",
        "#...K.#.....#.#.....#...#...#.#",
        "#####.#####.#.#####.#.#.###.#.#",
        "#.....^...#...#..M....#..K..#.#",
        "#.#######.#####.#######.#####D#",
        "#$..S.....^..........K..D.$.#E#",
        "###############################" } }
};
class Grid {
    int rows, cols;
    char** cells;
    void allocate(int r, int c) {
        rows = r; cols = c;
        cells = new char*[rows];
        for (int i = 0; i < rows; i++) cells[i] = new char[cols];
    }
    void release() {
        for (int i = 0; i < rows; i++) delete[] cells[i];
        delete[] cells;
    }
    void copyCells(const Grid& other) {
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) cells[r][c] = other.cells[r][c];
    }
public:
    Grid(int r = 1, int c = 1, char fill = WALL) {
        allocate(r, c);
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++) cells[i][j] = fill;
    }
    Grid(const Grid& other) { allocate(other.rows, other.cols); copyCells(other); }
    Grid& operator=(const Grid& other) {
        if (this != &other) {
            release();
            allocate(other.rows, other.cols);
            copyCells(other);
        }
        return *this;
    }
    ~Grid() { release(); }
    int getRows() const { return rows; }
    int getCols() const { return cols; }
    bool inside(int r, int c) const { return r >= 0 && r < rows && c >= 0 && c < cols; }
    char get(int r, int c) const { return inside(r, c) ? cells[r][c] : WALL; }
    void set(int r, int c, char value) { if (inside(r, c)) cells[r][c] = value; }
    int nodeCount() const { return rows * cols; }
    int nodeId(int r, int c) const { return r * cols + c; }
    int rowOf(int id) const { return id / cols; }
    int colOf(int id) const { return id % cols; }
};
class Queue {
    int values[500];
    int front, rear;
public:
    Queue() { front = rear = 0; }
    bool isEmpty() const { return front == rear; }
    void enqueue(int value) { values[rear++] = value; }
    int dequeue() { if (isEmpty()) return -1; return values[front++]; }
};
// Linked list stack of positions (Last In, First Out), used by Undo.
class Stack {
    struct Node { Position value; Node* next; Node(Position v, Node* n) : value(v), next(n) {} };
    Node* top;
    Stack(const Stack&); Stack& operator=(const Stack&);
public:
    Stack() : top(NULL) {}
    ~Stack() { clear(); }
    bool isEmpty() const { return top == NULL; }
    void push(Position value) { top = new Node(value, top); }
    bool pop(Position& value) {
        if (isEmpty()) return false;
        Node* oldTop = top;
        value = oldTop->value;
        top = top->next;
        delete oldTop;
        return true;
    }
    void clear() { Position ignored; while (pop(ignored)) {} }
};
// Singly linked list of the items the player is carrying.
class Inventory {
    struct Node { string item; Node* next; Node(string i) : item(i), next(NULL) {} };
    Node* head;
    Inventory(const Inventory&); Inventory& operator=(const Inventory&);
public:
    Inventory() : head(NULL) {}
 ~Inventory() { while (head != NULL) removeItem(head->item); }
    void addItem(string item) {
        Node* node = new Node(item);
        if (head == NULL) { head = node; return; }
        Node* current = head;
        while (current->next != NULL) current = current->next;
        current->next = node;
    }
    bool removeItem(string item) {
        Node* current = head;
        Node* previous = NULL;
        while (current != NULL && current->item != item) {
            previous = current;
            current = current->next;
        }
        if (current == NULL) return false;
        if (previous == NULL) head = current->next;
        else previous->next = current->next;
        delete current;
        return true;
    }
    int countItem(string item) const {
        int count = 0;
        for (Node* n = head; n != NULL; n = n->next)
            if (n->item == item) count++;
        return count;
    }
    string listItems() const {
        if (head == NULL) return "(empty)";
        string text = head->item;
        for (Node* n = head->next; n != NULL; n = n->next) text += ", " + n->item;
        return text;
    }
};
struct Record { string name; int score; int cleared; };
// Binary Search Tree of records. In-order traversal gives A-Z order.
class NameTree {
    struct Node {
        Record record; Node* left; Node* right;
        Node(const Record& r) : record(r), left(NULL), right(NULL) {}
    };
    Node* root;
    NameTree(const NameTree&); NameTree& operator=(const NameTree&);
    Node* insertAt(Node* node, const Record& record) {
        if (node == NULL) node = new Node(record);
        else if (record.name < node->record.name) node->left = insertAt(node->left, record);
        else if (record.name > node->record.name) node->right = insertAt(node->right, record);
        else node->record = record;  // same name: replace the old record
        return node;
    }
    void printAt(Node* node) const {
        if (node == NULL) return;
        printAt(node->left);
        cout << node->record.name << "\t" << node->record.score
             << "\t" << node->record.cleared << "\n";
        printAt(node->right);
    }
    void destroy(Node* node) {
        if (node == NULL) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }
public:
    NameTree() : root(NULL) {}
    ~NameTree() { destroy(root); }
    void insert(const Record& record) { root = insertAt(root, record); }
void printInOrder() const { printAt(root); }
    const Record* find(string name) const {
        Node* current = root;
        while (current != NULL) {
            if (name == current->record.name) return &current->record;
            current = (name < current->record.name) ? current->left : current->right;
        }
        return NULL;
    }
};
bool readLine(string& line) { return !getline(cin, line).fail(); }
void clearScreen() { system("cls"); }
void waitForEnter() { string ignored; cout << "\nPress Enter to continue..."; readLine(ignored); }
string trim(string text) {
    size_t first = 0, last = text.size();
    while (first < last && (text[first] == ' ' || text[first] == '\t')) first++;
    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t')) last--;
    return text.substr(first, last - first);
}
string cleanName(string name) {
    name = trim(name);
    name = trim(name.substr(0, MAX_NAME_LENGTH));
    return name.empty() ? "Player" : name;
}
class ScoreBoard {
    Record* records;
    int count, capacity;
    ScoreBoard(const ScoreBoard&); ScoreBoard& operator=(const ScoreBoard&);
    void grow() {
        capacity = capacity * 2;
        Record* bigger = new Record[capacity];
        for (int i = 0; i < count; i++) bigger[i] = records[i];
        delete[] records;
        records = bigger;
    }
public:
    ScoreBoard() : count(0), capacity(4) { records = new Record[capacity]; }
    ~ScoreBoard() { delete[] records; }
    int getCount() const { return count; }
    const Record& getRecord(int index) const { return records[index]; }
    // LINEAR SEARCH: returns the player's index, or -1 if not found
    int findPlayer(string name) const {
        for (int i = 0; i < count; i++)
            if (records[i].name == name) return i;
        return -1;
    }
    // INSERTION SORT: highest score first
    void sortByScore() {
        for (int i = 1; i < count; i++) {
            Record current = records[i];
            int j = i - 1;
            while (j >= 0 && (current.score > records[j].score ||
                   (current.score == records[j].score && current.cleared > records[j].cleared))) {
                records[j + 1] = records[j];
                j--;
            }
            records[j + 1] = current;
        }
    }
    void addScore(string name, int score, int cleared) {
        int index = findPlayer(name);
        if (index == -1) {
            if (count == capacity) grow();
            index = count++;
            records[index].name = name;
            records[index].score = -1;  // the check below will replace this
        }
        if (score > records[index].score) { records[index].score = score; records[index].cleared = cleared; }
        sortByScore();
    }
    void load() {
        ifstream file(SCORE_FILE);
        int score, cleared;
        string name;
        while (file >> score >> cleared) {
            getline(file, name);
            name = trim(name);
            if (score >= 0 && cleared >= 0 && !name.empty())
                addScore(name, score, cleared);
        }
    }
    bool save() const {
        ofstream file(SCORE_FILE);
        if (!file) return false;
        for (int i = 0; i < count; i++)
            file << records[i].score << " " << records[i].cleared << " " << records[i].name << "\n";
        return true;
    }
    void printRanking() const {
        cout << "Rank\tName\tScore\tCleared\n";
        for (int i = 0; i < count; i++)
            cout << i + 1 << "\t" << records[i].name << "\t"
                 << records[i].score << "\t" << records[i].cleared << "\n";
    }
    void fillTree(NameTree& tree) const { for (int i = 0; i < count; i++) tree.insert(records[i]); }
};
enum Status { PLAYING, LEVEL_WON, GAME_OVER, QUIT };
class Game {
public:
    Grid maze, hintView;
    bool hintVisible, stopInput;
    Position player, monster, playerStart, monsterStart;
    Stack history;
    Inventory bag;
    string message;
    int health, lives, score, moves, monsterDelay, level, bossHealth, crystals;
    bool bossMode;
    Game() : hintVisible(false), stopInput(false), health(START_HEALTH), lives(START_LIVES),
             score(0), moves(0), monsterDelay(1), level(0), bossHealth(100), crystals(0), bossMode(false) {}
    void prepareLevel(int number, int delay) {
        level = number; monsterDelay = delay; moves = 0; message = "";
        hintVisible = stopInput = false; respawn();
    }
    void loadLevel(int number) {
        const LevelData& data = LEVELS[number - 1];
        int cols = string(data.lines[0]).size();
        maze = Grid(data.rows, cols);  // uses the assignment operator
        for (int r = 0; r < data.rows; r++) {
            for (int c = 0; c < cols; c++) {
                char cell = data.lines[r][c];
                if (cell == 'P') playerStart = Position(r, c);
                if (cell == 'M') monsterStart = Position(r, c);
                maze.set(r, c, (cell == 'P' || cell == 'M') ? FLOOR : cell);
            }
        }
        prepareLevel(number, data.monsterDelay);
    }
    // Recursion and backtracking create the random maze by trying different directions.
    void carve(int r, int c) {
        maze.set(r, c, FLOOR);
        int order[4] = {0, 1, 2, 3};
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i + 1);
