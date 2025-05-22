#pragma once
#include "SinglePortModule.h"
#include <string>
#include <map>
#include <ctime>
#include <vector>

// Game state structure for Tic Tac Toe
struct TicTacToeGame {
    char board[9];
    uint32_t player1;
    uint32_t player2;
    uint32_t currentPlayer;
    time_t wasUpdated;  // When the game was updated/created
};

// Game state structure for Hangman
struct HangmanGame {
    std::string word;
    std::string guessedLetters;
    std::string currentState;  // Current state of the word with _ for unguessed letters
    uint32_t player;
    int remainingGuesses;
    time_t wasUpdated;
};

// Game state structure for Rock Paper Scissors
struct RPSGame {
    uint32_t player1;
    uint32_t player2;
    char player1Choice;  // 'R', 'P', or 'S'
    char player2Choice;  // 'R', 'P', or 'S'
    bool player1Ready;
    bool player2Ready;
    bool isBotGame;      // Whether this is a game against a bot
    time_t wasUpdated;
};

// Game state structure for Auto Chess
struct AutoChessUnit {
    std::string name;
    int level;          // 1-3 stars
    int cost;           // 1-5 gold
    int health;
    int damage;
    int mana;
    std::string race;   // e.g., "Human", "Elf", "Orc"
    std::string class_; // e.g., "Warrior", "Mage", "Assassin"
};

// Shop structure for Auto Chess
struct AutoChessShop {
    std::vector<AutoChessUnit> availableUnits;  // Current shop units
    time_t lastRefresh;  // When the shop was last refreshed
};

struct AutoChessPlayer {
    uint32_t playerId;
    int gold;           // Current gold
    int level;          // Player level (1-10)
    int experience;     // Current XP
    int mana;          // Current mana
    std::vector<AutoChessUnit> bench;    // Units waiting to be placed
    std::vector<AutoChessUnit> board;    // Units on the board
    AutoChessShop shop;  // Player's shop
    time_t wasUpdated;
};

struct AutoChessGame {
    std::map<uint32_t, AutoChessPlayer> players;
    int round;          // Current round
    bool isActive;      // Whether the game is active
    time_t wasUpdated;
};

class GamesModule : public SinglePortModule
{
  public:
    GamesModule() : SinglePortModule("games", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

  protected:
    virtual meshtastic_MeshPacket *allocReply() override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    std::map<uint32_t, TicTacToeGame> activeGames;
    std::map<uint32_t, HangmanGame> activeHangmanGames;
    std::map<uint32_t, RPSGame> activeRPSGames;
    std::map<uint32_t, AutoChessGame> activeAutoChessGames;  // Map of game ID to game state
    static const int GAME_TIMEOUT_SECONDS = 600; // 10 minutes
    
    // Word list for Hangman
    static const char* const HANGMAN_WORDS[];
    static const int HANGMAN_WORDS_COUNT = 50;  // Number of words in the list
    
    // Auto Chess game handlers
    bool handleAutoChessCommand(const meshtastic_MeshPacket &mp, const char *command);
    void startNewAutoChessGame(uint32_t player);
    bool joinAutoChessGame(uint32_t player, uint32_t gameId);
    void cleanupAutoChessGame(uint32_t gameId);
    std::string getAutoChessStateString(const AutoChessPlayer &player);
    bool buyUnit(AutoChessPlayer &player, int unitIndex);
    bool sellUnit(AutoChessPlayer &player, int unitIndex);
    bool placeUnit(AutoChessPlayer &player, int benchIndex, int boardIndex);
    void processRound(AutoChessGame &game);
    void distributeGold(AutoChessGame &game);
    void distributeMana(AutoChessGame &game);
    void checkLevelUp(AutoChessPlayer &player);
    void refreshShop(AutoChessPlayer &player);  // Refresh shop with new units
    std::string getShopString(const AutoChessPlayer &player);  // Get shop display string
    
    // Predefined units for the shop
    static const AutoChessUnit UNIT_TEMPLATES[];
    static const int UNIT_TEMPLATES_COUNT = 10;  // Number of different unit types
    
    // Game command handlers
    bool handleTicTacToeCommand(const meshtastic_MeshPacket &mp, const char *command);
    bool handleTicTacToeMove(const meshtastic_MeshPacket &mp, int position);
    void startNewTicTacToeGame(uint32_t player1, uint32_t player2);
    std::string getBoardString(const TicTacToeGame &game);
    bool checkWin(const TicTacToeGame &game);
    bool checkDraw(const TicTacToeGame &game);
    void cleanupOldGames();  // Clean up games older than GAME_TIMEOUT_SECONDS

    // Hangman game handlers
    bool handleHangmanCommand(const meshtastic_MeshPacket &mp, const char *command);
    void startNewHangmanGame(uint32_t player);
    std::string getHangmanStateString(const HangmanGame &game);
    bool makeHangmanGuess(const meshtastic_MeshPacket &mp, char guess);
    bool checkHangmanWin(const HangmanGame &game);
    std::string getRandomWord();

    // Rock Paper Scissors game handlers
    bool handleRPSCommand(const meshtastic_MeshPacket &mp, const char *command);
    void startNewRPSGame(uint32_t player1, uint32_t player2, bool isBotGame = false);
    bool makeRPSChoice(const meshtastic_MeshPacket &mp, char choice);
    std::string getRPSResult(const RPSGame &game);
    void cleanupRPSGame(uint32_t gameId);
    char getBotChoice();  // Get a random choice for the bot

    // Battle processing functions
    void processBattles(AutoChessGame &game);
    void processBattle(AutoChessGame &game, uint32_t player1Id, uint32_t player2Id);
    void sendBattleResults(uint32_t playerId, int round, bool won, int healthLost,
                          bool hasWarrior, bool hasTroll, bool hasElf,
                          bool hasKnight, bool hasMage);
    int calculateTeamHealth(const std::vector<AutoChessUnit> &team);

    // Unit counting functions
    int countWarriors(const AutoChessPlayer &player);
    int countTrolls(const AutoChessPlayer &player);
    int countElves(const AutoChessPlayer &player);
    int countKnights(const AutoChessPlayer &player);
    int countMages(const AutoChessPlayer &player);
    bool shouldDodge();
}; 