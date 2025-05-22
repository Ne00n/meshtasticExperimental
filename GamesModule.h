#pragma once
#include "SinglePortModule.h"
#include <string>
#include <map>
#include <ctime>

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
    static const int GAME_TIMEOUT_SECONDS = 600; // 10 minutes
    
    // Word list for Hangman
    static const char* const HANGMAN_WORDS[];
    static const int HANGMAN_WORDS_COUNT = 50;  // Number of words in the list
    
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
}; 