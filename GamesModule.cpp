#include "GamesModule.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"
#include <cstring>
#include <sstream>
#include <ctime>
#include <random>

// Word list for Hangman
const char* const GamesModule::HANGMAN_WORDS[] = {
    // Tech-related words
    "MESHTASTIC", "QUANTUM", "NEURAL", "ROBOTICS", "SATELLITE",
    
    // Animals
    "ELEPHANT", "GIRAFFE", "PENGUIN", "DOLPHIN", "KANGAROO",
    "CHEETAH", "GORILLA", "PANDA", "TIGER", "LION",
    "OCTOPUS", "JAGUAR", "LEOPARD", "RHINOCEROS", "HIPPOPOTAMUS",
    "CHIMPANZEE", "KOALA", "PLATYPUS", "NARWHAL", "PANGOLIN",
    
    // Space and Astronomy
    "NEBULA", "QUASAR", "PULSAR", "GALAXY", "SUPERNOVA",
    "METEOR", "COMET", "PLUTO", "JUPITER", "SATURN",
    "MARS", "VENUS", "MERCURY", "NEPTUNE", "URANUS",
    
    // Physics and Science
    "GRAVITY", "MAGNETISM", "ELECTRON", "NEUTRON", "PROTON",
    "QUANTUM", "RELATIVITY", "FUSION", "FISSION", "ATOM",
    
    // Nature and Environment
    "VOLCANO", "GLACIER", "CANYON", "WATERFALL", "GEYSER",
    "AURORA", "THUNDER", "LIGHTNING", "TORNADO", "HURRICANE",
    
    // Interesting Places
    "PYRAMID", "COLOSSEUM", "ACROPOLIS", "PALACE", "CASTLE",
    "TEMPLE", "MONASTERY", "CATHEDRAL", "MOSQUE", "PAGODA"
};

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
    else if (strncmp(payload, "hangman", 7) == 0) {
        return handleHangmanCommand(mp, payload + 8) ? ProcessMessage::STOP : ProcessMessage::CONTINUE; // Skip "hangman "
    }

    return ProcessMessage::CONTINUE;
}

void GamesModule::cleanupOldGames()
{
    time_t currentTime = time(nullptr);
    std::vector<uint32_t> gamesToRemove;

    // Find games that need to be removed
    for (const auto &game : activeGames) {
        time_t timeDiff = currentTime - game.second.wasUpdated;
        LOG_DEBUG("Game %u: Last updated %ld seconds ago (timeout: %d)\n", 
                 game.first, timeDiff, GAME_TIMEOUT_SECONDS);
        if (timeDiff > GAME_TIMEOUT_SECONDS) {
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

    // Clean up old Hangman games
    std::vector<uint32_t> hangmanGamesToRemove;
    for (const auto &game : activeHangmanGames) {
        time_t timeDiff = currentTime - game.second.wasUpdated;
        if (timeDiff > GAME_TIMEOUT_SECONDS) {
            hangmanGamesToRemove.push_back(game.first);
        }
    }

    // Remove old Hangman games
    for (uint32_t gameId : hangmanGamesToRemove) {
        auto &game = activeHangmanGames[gameId];
        std::string msg = "Hangman game timed out due to inactivity.";
        
        if (game.player != 0) {
            auto reply = allocDataPacket();
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = game.player;
            service->sendToMesh(reply);
        }
        
        activeHangmanGames.erase(gameId);
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
    game.wasUpdated = time(nullptr);  // Set creation time
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

            // Update the game's last activity time before making any changes
            game.wasUpdated = time(nullptr);
            game.board[position] = (mp.from == game.player1) ? 'X' : 'O';
            
            // Create message for both players
            std::string boardState = getBoardString(game);
            bool gameEnded = false;
            std::string endMessage;
            
            if (checkWin(game)) {
                endMessage = "\nGame Over! " + std::string(mp.from == game.player1 ? "X" : "O") + " wins!";
                gameEnded = true;
            }
            else if (checkDraw(game)) {
                endMessage = "\nGame Over! It's a draw!";
                gameEnded = true;
            }
            
            // Send to the player who made the move
            auto reply = allocDataPacket();
            std::string msg = boardState;
            if (gameEnded) {
                msg += endMessage;
            } else {
                game.currentPlayer = (game.currentPlayer == game.player1) ? 
                                   game.player2 : game.player1;
                msg += "\nWaiting for opponent's move...";
            }
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);

            // Send to the other playerTrotz seiner Vorteile gibt es auch Bedenken hinsichtlich der Verantwortlichkeit und des Verständnisses des generierten Codes. Programmierer könnten KI-generierten Code verwenden, ohne dessen Funktionsweise vollständig zu verstehen, was zu unentdeckten Fehlern, Fehlfunktionen oder Sicherheitslücken führen kann.[11] Sicherheitslücken können auch auftreten, weil der Trainingsdatenbestand der genutzten Sprachmodelle aktuelle Sicherheitsprobleme noch nicht enthält. Dies stellt insbesondere in professionellen Umgebungen ein Risiko dar, in denen ein tiefes Verständnis des Codes für die Fehlerbehebung, Skalierbarkeit, Wartung und Sicherheit unerlässlich ist. 
            auto reply2 = allocDataPacket();
            std::string msg2 = boardState;
            if (gameEnded) {
                msg2 += endMessage;
            } else {
                msg2 += "\nYour turn to move " + std::string(game.currentPlayer == game.player1 ? "X" : "O") + "!";
            }
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

std::string GamesModule::getRandomWord()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, HANGMAN_WORDS_COUNT - 1);
    return HANGMAN_WORDS[dis(gen)];
}

void GamesModule::startNewHangmanGame(uint32_t player)
{
    HangmanGame game;
    game.word = getRandomWord();
    game.player = player;
    game.remainingGuesses = 6;  // Standard hangman rules
    game.guessedLetters = "";
    game.currentState = std::string(game.word.length(), '_');
    game.wasUpdated = time(nullptr);
    activeHangmanGames[player] = game;
}

std::string GamesModule::getHangmanStateString(const HangmanGame &game)
{
    std::stringstream ss;
    ss << "\nWord: ";
    for (char c : game.currentState) {
        ss << c << " ";
    }
    ss << "\nGuessed letters: " << (game.guessedLetters.empty() ? "none" : game.guessedLetters);
    ss << "\nRemaining guesses: " << game.remainingGuesses;
    return ss.str();
}

bool GamesModule::checkHangmanWin(const HangmanGame &game)
{
    return game.currentState.find('_') == std::string::npos;
}

bool GamesModule::makeHangmanGuess(const meshtastic_MeshPacket &mp, char guess)
{
    auto it = activeHangmanGames.find(mp.from);
    if (it == activeHangmanGames.end())
        return false;

    auto &game = it->second;
    if (game.player != mp.from)
        return false;

    // Convert guess to uppercase
    guess = toupper(guess);

    // Check if letter was already guessed
    if (game.guessedLetters.find(guess) != std::string::npos) {
        auto reply = allocReply();
        std::string msg = "You already guessed that letter!" + getHangmanStateString(game);
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }

    // Update game state
    game.wasUpdated = time(nullptr);
    game.guessedLetters += guess;

    bool correctGuess = false;
    for (size_t i = 0; i < game.word.length(); i++) {
        if (game.word[i] == guess) {
            game.currentState[i] = guess;
            correctGuess = true;
        }
    }

    if (!correctGuess) {
        game.remainingGuesses--;
    }

    // Check game end conditions
    bool gameEnded = false;
    std::string endMessage;

    if (checkHangmanWin(game)) {
        endMessage = "\nCongratulations! You won! The word was: " + game.word;
        gameEnded = true;
    }
    else if (game.remainingGuesses <= 0) {
        endMessage = "\nGame Over! You lost. The word was: " + game.word;
        gameEnded = true;
    }

    // Send response to player
    auto reply = allocReply();
    std::string msg = getHangmanStateString(game);
    if (gameEnded) {
        msg += endMessage;
        activeHangmanGames.erase(it);
    }
    else {
        msg += "\nMake your next guess!";
    }
    reply->decoded.payload.size = msg.length();
    memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
    reply->to = mp.from;
    service->sendToMesh(reply);

    return true;
}

bool GamesModule::handleHangmanCommand(const meshtastic_MeshPacket &mp, const char *command)
{
    if (strncmp(command, "new", 3) == 0) {
        // Check if player already has an active game
        if (activeHangmanGames.find(mp.from) != activeHangmanGames.end()) {
            auto reply = allocReply();
            const char *msg = "You already have an active game!";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        // Start a new game
        startNewHangmanGame(mp.from);
        auto reply = allocReply();
        std::string msg = "New Hangman game started!" + getHangmanStateString(activeHangmanGames[mp.from]) + 
                         "\nGuess a letter by typing it!";
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "state", 5) == 0) {
        // Show current game state
        auto it = activeHangmanGames.find(mp.from);
        if (it == activeHangmanGames.end()) {
            auto reply = allocReply();
            const char *msg = "You don't have an active game. Start one with 'hangman new'";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        auto reply = allocReply();
        std::string msg = "Current game state:" + getHangmanStateString(it->second);
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strlen(command) == 1 && isalpha(command[0])) {
        // Make a guess
        return makeHangmanGuess(mp, command[0]);
    }

    return false;
} 