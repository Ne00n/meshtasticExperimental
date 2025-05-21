#include "GamesModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"
#include <cstring>
#include <sstream>

meshtastic_MeshPacket *GamesModule::allocReply()
{
    assert(currentRequest);
    auto reply = allocDataPacket();
    return reply;
}

bool GamesModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size == 0)
        return false;

    // Convert payload to null-terminated string
    char payload[mp.decoded.payload.size + 1];
    memcpy(payload, mp.decoded.payload.bytes, mp.decoded.payload.size);
    payload[mp.decoded.payload.size] = 0;

    // Handle different game commands
    if (strncmp(payload, "ttt", 3) == 0) {
        return handleTicTacToeCommand(mp, payload + 4); // Skip "ttt "
    }

    return false;
}

bool GamesModule::handleTicTacToeCommand(const meshtastic_MeshPacket &mp, const char *command)
{
    if (strncmp(command, "new", 3) == 0) {
        // Start a new game
        startNewTicTacToeGame(mp.from, 0); // Second player will be set when they join
        auto reply = allocReply();
        const char *msg = "New Tic Tac Toe game started! Waiting for opponent...";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        service.sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "join", 4) == 0) {
        // Join an existing game
        for (auto &game : activeGames) {
            if (game.second.player2 == 0 && game.second.player1 != mp.from) {
                game.second.player2 = mp.from;
                game.second.currentPlayer = game.second.player1;
                auto reply = allocReply();
                std::string msg = "Game started! Your turn.\n" + getBoardString(game.second);
                reply->decoded.payload.size = msg.length();
                memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                service.sendToMesh(reply);
                return true;
            }
        }
        return false;
    }
    else if (isdigit(command[0])) {
        // Make a move
        int position = command[0] - '0';
        return handleTicTacToeMove(mp, position);
    }

    return false;
}

void GamesModule::startNewTicTacToeGame(uint32_t player1, uint32_t player2)
{
    TicTacToeGame game;
    memset(game.board, ' ', sizeof(game.board));
    game.player1 = player1;
    game.player2 = player2;
    game.currentPlayer = player1;
    game.isActive = true;
    activeGames[player1] = game;
}

bool GamesModule::handleTicTacToeMove(const meshtastic_MeshPacket &mp, int position)
{
    if (position < 0 || position > 8)
        return false;

    for (auto &game : activeGames) {
        if ((game.second.player1 == mp.from || game.second.player2 == mp.from) && 
            game.second.currentPlayer == mp.from && 
            game.second.board[position] == ' ') {
            
            game.second.board[position] = (mp.from == game.second.player1) ? 'X' : 'O';
            
            auto reply = allocReply();
            std::string msg = getBoardString(game.second);
            
            if (checkWin(game.second)) {
                msg += "\nGame Over! Player " + std::to_string(mp.from) + " wins!";
                game.second.isActive = false;
            }
            else if (checkDraw(game.second)) {
                msg += "\nGame Over! It's a draw!";
                game.second.isActive = false;
            }
            else {
                game.second.currentPlayer = (game.second.currentPlayer == game.second.player1) ? 
                                          game.second.player2 : game.second.player1;
                msg += "\nNext player's turn.";
            }
            
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            service.sendToMesh(reply);
            return true;
        }
    }
    return false;
}

std::string GamesModule::getBoardString(const TicTacToeGame &game)
{
    std::stringstream ss;
    ss << "\n";
    for (int i = 0; i < 9; i += 3) {
        ss << " " << game.board[i] << " | " << game.board[i+1] << " | " << game.board[i+2] << "\n";
        if (i < 6) ss << "---+---+---\n";
    }
    return ss.str();
}

bool GamesModule::checkWin(const TicTacToeGame &game)
{
    // Check rows
    for (int i = 0; i < 9; i += 3) {
        if (game.board[i] != ' ' && game.board[i] == game.board[i+1] && game.board[i] == game.board[i+2])
            return true;
    }
    
    // Check columns
    for (int i = 0; i < 3; i++) {
        if (game.board[i] != ' ' && game.board[i] == game.board[i+3] && game.board[i] == game.board[i+6])
            return true;
    }
    
    // Check diagonals
    if (game.board[0] != ' ' && game.board[0] == game.board[4] && game.board[0] == game.board[8])
        return true;
    if (game.board[2] != ' ' && game.board[2] == game.board[4] && game.board[2] == game.board[6])
        return true;
    
    return false;
}

bool GamesModule::checkDraw(const TicTacToeGame &game)
{
    for (int i = 0; i < 9; i++) {
        if (game.board[i] == ' ')
            return false;
    }
    return true;
} 