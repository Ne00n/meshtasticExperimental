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
    bool isActive;
    time_t createdAt;  // When the game was created
};

class GamesModule : public SinglePortModule
{
  public:
    GamesModule() : SinglePortModule("games", meshtastic_PortNum_TEXT_MESSAGE_APP) {}

  protected:
    virtual meshtastic_MeshPacket *allocReply() override;
    virtual bool handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    std::map<uint32_t, TicTacToeGame> activeGames;
    static const int GAME_TIMEOUT_SECONDS = 600; // 10 minutes
    
    // Game command handlers
    bool handleTicTacToeCommand(const meshtastic_MeshPacket &mp, const char *command);
    bool handleTicTacToeMove(const meshtastic_MeshPacket &mp, int position);
    void startNewTicTacToeGame(uint32_t player1, uint32_t player2);
    std::string getBoardString(const TicTacToeGame &game);
    bool checkWin(const TicTacToeGame &game);
    bool checkDraw(const TicTacToeGame &game);
    void cleanupOldGames();  // Clean up games older than GAME_TIMEOUT_SECONDS
}; 