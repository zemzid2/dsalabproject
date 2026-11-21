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
            int temp = order[i];
            order[i] = order[j];
            order[j] = temp;
        }
        for (int i = 0; i < 4; i++) {
            int nextRow = r + 2 * ROW_CHANGE[order[i]], nextCol = c + 2 * COL_CHANGE[order[i]];
            bool inArea = nextRow >= 1 && nextRow <= maze.getRows() - 2 &&
                          nextCol >= 1 && nextCol <= maze.getCols() - 4;
            if (inArea && maze.get(nextRow, nextCol) == WALL) {
                maze.set(r + ROW_CHANGE[order[i]], c + COL_CHANGE[order[i]], FLOOR);
                carve(nextRow, nextCol);
            }
        }
    }
    void openWalls(int count, char value) {
        for (int tries = 0; count > 0 && tries < 5000; tries++) {
            int r = 1 + rand() % (maze.getRows() - 2), c = 1 + rand() % (maze.getCols() - 4);
            bool across = maze.get(r, c - 1) == FLOOR && maze.get(r, c + 1) == FLOOR;
            bool down = maze.get(r - 1, c) == FLOOR && maze.get(r + 1, c) == FLOOR;
            if (maze.get(r, c) == WALL && (across || down)) { maze.set(r, c, value); count--; }
        }
    }
    void placeItems(char item, int count) {
        while (count > 0) {
            int r = rand() % maze.getRows(), c = rand() % maze.getCols();
            bool isStart = (r == playerStart.row && c == playerStart.col) ||
                           (r == monsterStart.row && c == monsterStart.col);
            if (maze.get(r, c) == FLOOR && !isStart) { maze.set(r, c, item); count--; }
        }
    }
    void generateRandomLevel() {
        const int ROWS = 13, COLS = 25;
        maze = Grid(ROWS, COLS, WALL);
        carve(1, 1);
        openWalls(25, FLOOR); openWalls(2, HIDDEN);
        maze.set(ROWS - 2, COLS - 3, DOOR); maze.set(ROWS - 2, COLS - 2, EXIT);
        playerStart = Position(1, 1); monsterStart = Position(ROWS - 2, COLS - 4);
        placeItems(KEY, 1); placeItems(TREASURE, 3); placeItems(TRAP, 3);
        prepareLevel(0, 2);
    }
    bool canEnter(int r, int c, bool forMonster) const {
        char cell = maze.get(r, c);
        if (cell == WALL || cell == HIDDEN) return false;
        if (cell == FOUND) return !forMonster;
        if (cell == DOOR) return !forMonster && bag.countItem("Key") > 0;
        return true;
    }
    // BFS checks nearby cells first, so the first route it finds is a shortest route.
    int bfs(Position start, bool forMonster, int* parent, int* dist) const {
        for (int i = 0; i < maze.nodeCount(); i++) parent[i] = dist[i] = -1;
        Queue queue;
        dist[maze.nodeId(start.row, start.col)] = 0;
        queue.enqueue(maze.nodeId(start.row, start.col));
        int explored = 0;
        while (!queue.isEmpty()) {
            int node = queue.dequeue();
            explored++;
            for (int i = 0; i < 4; i++) {
                int r = maze.rowOf(node) + ROW_CHANGE[i], c = maze.colOf(node) + COL_CHANGE[i];
                if (canEnter(r, c, forMonster) && dist[maze.nodeId(r, c)] == -1) {
                    int next = maze.nodeId(r, c);
                    dist[next] = dist[node] + 1;
                    parent[next] = node;
                    queue.enqueue(next);
                }
            }
        }
 return explored;
    }
        void moveMonster() {
        int* parent = new int[maze.nodeCount()];
        int* dist = new int[maze.nodeCount()];
        bfs(monster, true, parent, dist);
        int node = maze.nodeId(player.row, player.col);
        if (dist[node] > 0) {  // the player can be reached
            while (dist[node] > 1) node = parent[node];  // back to the first step
            monster = Position(maze.rowOf(node), maze.colOf(node));
        }
        delete[] parent;
        delete[] dist;
    }
    void losePoints(int points) { score = (score > points) ? score - points : 0; }
    void respawn() { player = playerStart; monster = monsterStart; history.clear(); }
    Status takeDamage(int amount) {
        health -= amount;
        stopInput = true;
        if (health > 0) return PLAYING;
        lives--;
        if (lives <= 0) {
            health = 0;
            message += "You have no lives left!\n";
            return GAME_OVER;
        }
        health = START_HEALTH;
        message += "You lost a life! Back to the start.\n";
        respawn();
        return PLAYING;
    }
    bool monsterCaughtPlayer() const { return player.row == monster.row && player.col == monster.col; }
    Status catchPlayer() {
        message += "The monster caught you!\n";
        Status status = takeDamage(MONSTER_DAMAGE);
        if (status == PLAYING) respawn();
        return status;
    }
    Status endTurn() {
        if (monsterCaughtPlayer()) return catchPlayer();
        if (moves % monsterDelay == 0) {
            moveMonster();
            if (monsterCaughtPlayer()) return catchPlayer();
        }
        for (int i = 0; i < 4; i++) {
            if (maze.get(player.row + ROW_CHANGE[i], player.col + COL_CHANGE[i]) == HIDDEN) {
                message += "You feel a cold draft from a nearby wall...\n";
                break;
            }
        }
        return PLAYING;
    }
    Status blocked(string text) { message += text + "\n"; stopInput = true; return PLAYING; }
    Status collectFromCell() {
        char cell = maze.get(player.row, player.col);
        if (cell != FOUND) maze.set(player.row, player.col, FLOOR);  // items and traps work once
        if (cell == KEY) {
            bag.addItem("Key");
            message += "You picked up a key.\n";
        } else if (cell == TREASURE) {
            bag.addItem("Treasure");
            score += TREASURE_POINTS;
            message += "You found treasure!\n";
        } else if (cell == HIDDEN) {
            maze.set(player.row, player.col, FOUND);
            message += "You found a secret passage! The monster cannot use it.\n";
        } else if (cell == TRAP) {
            message += "A trap! It hurts.\n";
            return takeDamage(TRAP_DAMAGE);
        } else if (cell == CRYSTAL) {
            crystals++;
            bossHealth -= 25;
            score += TREASURE_POINTS;
            message += "Crystal collected! The boss is weaker.\n";
        }
        return PLAYING;
    }
    Status movePlayer(int rowChange, int colChange) {
        int r = player.row + rowChange, c = player.col + colChange;
        char cell = maze.get(r, c);
        if (cell == WALL) return blocked("A wall blocks your way.");
        if (cell == EXIT && bossMode && bossHealth > 0)
            return blocked("Defeat the boss before escaping.");
        if (cell == DOOR) {
            if (!bag.removeItem("Key")) return blocked("The door is locked. You need a key.");
            maze.set(r, c, FLOOR);
            message += "You unlocked the door.\n";
        }
        history.push(player);
        player = Position(r, c);
        moves++;
        if (cell == EXIT) return LEVEL_WON;
        if (collectFromCell() == GAME_OVER) return GAME_OVER;
        return endTurn();
    }
    Status fightBoss() {
        if (!bossMode) return blocked("There is no boss here.");
        if (crystals < 3) return blocked("Find all 3 crystals first.");
        int distance = abs(player.row - monster.row) + abs(player.col - monster.col);
        if (distance != 1) return blocked("Move next to the boss to attack.");
        bossHealth -= 25;
        moves++;
        if (bossHealth <= 0) {
            message += "You defeated the boss! Find the exit.\n";
            monster = Position(-1, -1);
            return PLAYING;
        }
        message += "You hit the boss!\n";
        return endTurn();
    }
    // UNDO: returns the player to the previous cell. Collected items stay collected.
    Status undoMove() {
        Position previous;
        if (!history.pop(previous)) return blocked("There is nothing to undo.");
        player = previous;
        moves++;
        losePoints(UNDO_COST);
        message += "Move undone (-5 points).\n";
        return endTurn();
    }
    // HINT: finds a shortest route to the exit, or to a key if the exit is locked.
    void showHint() {
        int* parent = new int[maze.nodeCount()]; int* dist = new int[maze.nodeCount()];
        bfs(player, false, parent, dist);
        int exitNode = -1, keyNode = -1;
        for (int node = 0; node < maze.nodeCount(); node++) {
            char cell = maze.get(maze.rowOf(node), maze.colOf(node));
            if (dist[node] == -1) continue;
            if (cell == EXIT) exitNode = node;
            if (cell == KEY && (keyNode == -1 || dist[node] < dist[keyNode])) keyNode = node;
        }
        int goal = (exitNode != -1) ? exitNode : keyNode;
        if (goal == -1) message += "Hint: no route is known. Look for a secret passage.\n";
        else {
            hintView = maze;  // uses the assignment operator
            for (int node = parent[goal]; node != -1; node = parent[node]) {
                char cell = maze.get(maze.rowOf(node), maze.colOf(node));
                if (cell == FLOOR || cell == FOUND) hintView.set(maze.rowOf(node), maze.colOf(node), ROUTE);
            }
            hintVisible = true;
            losePoints(HINT_COST);
            if (exitNode != -1) message += "Hint: follow the * to the exit (-10 points).\n";
            else message += "Hint: you need a key! Follow the * (-10 points).\n";
        }
        delete[] parent;
        delete[] dist;
    }
    Status handleCommand(char command) {
        if (command >= 'A' && command <= 'Z') command = command + 32;
        switch (command) {
            case 'w': return movePlayer(-1, 0);
            case 's': return movePlayer(1, 0);
            case 'a': return movePlayer(0, -1);
            case 'd': return movePlayer(0, 1);
            case 'u': return undoMove();
            case 'f': return fightBoss();
            case 'q': return QUIT;
            case 'h': showHint(); break;
            case 'i': message += "Inventory: " + bag.listItems() + "\n"; break;
            case ' ': break;
            default: return blocked("Unknown command. Use W, A, S, D, H, U, I or Q.");
        }
        return PLAYING;
    }
    char symbolAt(int r, int c) const {
        if (r == player.row && c == player.col) return '@';
        if (r == monster.row && c == monster.col) return 'M';
        if (hintVisible && hintView.get(r, c) == ROUTE) return ROUTE;
        char cell = maze.get(r, c);
        if (cell == HIDDEN) return WALL;  // secret passages look like walls
        return (cell == FLOOR) ? ' ' : cell;
    }
    void draw() const {
        clearScreen();
        cout << "================ THE HAUNTED MAZE ================\n";
        if (level == 0) cout << "Level: Random";
        else cout << "Level: " << level << "/" << LEVEL_COUNT;
        cout << "  Health: " << health << "  Lives: " << lives
             << "  Score: " << score << "  Moves: " << moves << "\n"
             << "Keys: " << bag.countItem("Key") << "  Treasure: " << bag.countItem("Treasure") << "\n";
        if (bossMode) cout << "Crystals: " << crystals << "/3  Boss health: " << bossHealth << "\n";
        cout << "\n";
        for (int r = 0; r < maze.getRows(); r++) {
            for (int c = 0; c < maze.getCols(); c++) cout << symbolAt(r, c);
            cout << "\n";
        }
        cout << "\n@ You  M " << (bossMode ? "Boss" : "Monster") << "  K Key  D Door  $ Treasure  ^ Trap  E Exit\n"
             << "W/A/S/D Move  H Hint  U Undo  I Inventory  Q Quit\n";
        if (bossMode) cout << "F Attack boss (stand next to it)\n";
        cout << "(You can type several moves at once, e.g. ddds, then press Enter)\n\n"
             << message;
    }
    Status playLevel() {
        Status status = PLAYING;
        while (status == PLAYING) {
            draw();
            cout << "Command: ";
            string line;
            if (!readLine(line)) return QUIT;
