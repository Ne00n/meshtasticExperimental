#include "GamesModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"
#include <cstring>
#include <sstream>
#include <ctime>

meshtastic_MeshPacket *GamesModule::allocReply()
{
    assert(currentRequest);
    auto reply = allocDataPacket();
    return reply;
}

ProcessMessage GamesModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size == 0)
        return ProcessMessage::CONTINUE;

    // Clean up old games before processing new commands
    cleanupOldGames();

    // Convert payload to null-terminated string
    char payload[mp.decoded.payload.size + 1];
    memcpy(payload, mp.decoded.payload.bytes, mp.decoded.payload.size);
    payload[mp.decoded.payload.size] = 0;

    // Handle different game commands
    if (strncmp(payload, "ttt", 3) == 0) {
        return handleTicTacToeCommand(mp, payload + 4) ? ProcessMessage::STOP : ProcessMessage::CONTINUE; // Skip "ttt "
    }

    return ProcessMessage::CONTINUE;
}

void GamesModule::cleanupOldGames()
{
    time_t currentTime = time(nullptr);
    std::vector<uint32_t> gamesToRemove;

    // Find games that need to be removed
    for (const auto &game : activeGames) {
        if (currentTime - game.second.updated > GAME_TIMEOUT_SECONDS) {
            gamesToRemove.push_back(game.first);
        }
    }

    // Remove the old games
    for (uint32_t gameId : gamesToRemove) {
        auto &game = activeGames[gameId];
        std::string msg = "Game timed out due to inactivity.";
        
        // Notify player 1 if they exist
        if (game.player1 != 0) {
            auto reply = allocDataPacket();
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = game.player1;
            service->sendToMesh(reply);
        }
        
        // Notify player 2 if they exist
        if (game.player2 != 0) {
            auto reply = allocDataPacket();
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = game.player2;
            service->sendToMesh(reply);
        }
        
        activeGames.erase(gameId);
    }
}

bool GamesModule::handleTicTacToeCommand(const meshtastic_MeshPacket &mp, const char *command)
{
    if (strncmp(command, "new", 3) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from; // Ensure reply goes only to the sender
                service->sendToMesh(reply);
                return true;
            }
        }

        // Start a new game
        startNewTicTacToeGame(mp.from, 0); // Second player will be set when they join
        auto reply = allocReply();
        const char *msg = "New Tic Tac Toe game started! Waiting for an opponent to join...\nUse 'ttt board' to check the game state if you miss any updates.";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from; // Ensure reply goes only to the sender
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "join", 4) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from; // Ensure reply goes only to the sender
                service->sendToMesh(reply);
                return true;
            }
        }

        // List available games
        std::vector<uint32_t> availableGames;
        for (const auto &game : activeGames) {
            if (game.second.player2 == 0 && game.second.player1 != mp.from) {
                availableGames.push_back(game.first);
            }
        }

        if (availableGames.empty()) {
            auto reply = allocReply();
            const char *msg = "No games available to join. Start a new game with 'ttt new'";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from; // Ensure reply goes only to the sender
            service->sendToMesh(reply);
            return true;
        }

        // Join the first available game
        auto &game = activeGames[availableGames[0]];
        game.player2 = mp.from;
        game.currentPlayer = game.player1;

        // Notify both players
        auto reply = allocReply();
        std::string msg = "Game started! You are O. Waiting for opponent's move...\n" + getBoardString(game);
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = mp.from; // Send to the joining player
        service->sendToMesh(reply);

        // Notify the first player
        auto reply2 = allocDataPacket(); // Use allocDataPacket for the second message
        std::string msg2 = "Opponent joined! You are X. Your turn to move X!\n" + getBoardString(game);
        reply2->decoded.payload.size = msg2.length();
        memcpy(reply2->decoded.payload.bytes, msg2.c_str(), reply2->decoded.payload.size);
        reply2->to = game.player1;
        service->sendToMesh(reply2);

        return true;
    }
    else if (strncmp(command, "board", 5) == 0) {
        // Find player's active game
        for (const auto &game : activeGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                std::string msg = "Current game state:\n" + getBoardString(game.second);
                if (game.second.currentPlayer == mp.from) {
                    msg += "\nYour turn to move " + std::string(game.second.currentPlayer == game.second.player1 ? "X" : "O") + "!";
                } else {
                    msg += "\nWaiting for opponent's move...";
                }
                reply->decoded.payload.size = msg.length();
                memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }
        
        // No active game found
        auto reply = allocReply();
        const char *msg = "You don't have an active game. Start one with 'ttt new' or join one with 'ttt join'";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (isdigit(command[0])) {
        // Make a move
        int position = command[0] - '1'; // Convert from 1-9 to 0-8
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
    game.updated = time(nullptr);  // Set creation time
    activeGames[player1] = game;
}

bool GamesModule::handleTicTacToeMove(const meshtastic_MeshPacket &mp, int position)
{
    if (position < 0 || position > 8)
        return false;

    for (auto it = activeGames.begin(); it != activeGames.end(); ++it) {
        auto &game = it->second;
        if ((game.player1 == mp.from || game.player2 == mp.from) && 
            game.currentPlayer == mp.from && 
            game.board[position] == ' ') {
            game.board[position] = (mp.from == game.player1) ? 'X' : 'O';

            // Update the game's last activity time
            game.createdAt = time(nullptr);
            
            // Create message for both players
            std::string msg = getBoardString(game);
            bool gameEnded = false;
            
            if (checkWin(game)) {
                msg += "\nGame Over! " + std::string(mp.from == game.player1 ? "X" : "O") + " wins!";
                gameEnded = true;
            }
            else if (checkDraw(game)) {
                msg += "\nGame Over! It's a draw!";
                gameEnded = true;
            }
            else {
                game.currentPlayer = (game.currentPlayer == game.player1) ? 
                                   game.player2 : game.player1;
                msg += "\nWaiting for opponent's move...";
            }
            
            // Send to the player who made the move
            auto reply = allocDataPacket();
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);

            // Send to the other player
            auto reply2 = allocDataPacket();
            std::string msg2 = getBoardString(game) + "\nYour turn to move " + std::string(game.currentPlayer == game.player1 ? "X" : "O") + "!";
            reply2->decoded.payload.size = msg2.length();
            memcpy(reply2->decoded.payload.bytes, msg2.c_str(), reply2->decoded.payload.size);
            reply2->to = (mp.from == game.player1) ? game.player2 : game.player1;
            service->sendToMesh(reply2);

            // Remove the game if it ended
            if (gameEnded) {
                activeGames.erase(it);
            }

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
        ss << " " << (game.board[i] == ' ' ? std::to_string(i+1) : std::string(1, game.board[i])) << " | "
           << (game.board[i+1] == ' ' ? std::to_string(i+2) : std::string(1, game.board[i+1])) << " | "
           << (game.board[i+2] == ' ' ? std::to_string(i+3) : std::string(1, game.board[i+2])) << "\n";
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