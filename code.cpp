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
