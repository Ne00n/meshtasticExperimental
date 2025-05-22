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

// Predefined units for Auto Chess
const AutoChessUnit GamesModule::UNIT_TEMPLATES[] = {
    {"Knight", 1, 3, 100, 15, 50, "Human", "Warrior"},
    {"Archer", 1, 2, 70, 20, 40, "Elf", "Ranger"},
    {"Mage", 1, 4, 60, 25, 80, "Human", "Mage"},
    {"Orc Warrior", 1, 3, 120, 18, 30, "Orc", "Warrior"},
    {"Druid", 1, 3, 80, 15, 60, "Elf", "Mage"},
    {"Assassin", 1, 4, 65, 30, 50, "Human", "Assassin"},
    {"Troll", 1, 2, 90, 12, 40, "Orc", "Warrior"},
    {"Priest", 1, 3, 75, 10, 70, "Human", "Mage"},
    {"Ranger", 1, 2, 70, 18, 45, "Elf", "Ranger"},
    {"Berserker", 1, 4, 110, 25, 35, "Orc", "Warrior"}
};

// Add these constants at the top with other constants
const int BATTLE_INTERVAL_SECONDS = 30;
const int WARRIOR_SYNERGY_THRESHOLD = 3;
const float WARRIOR_DAMAGE_REDUCTION = 0.2f; // 20% damage reduction for 3+ warriors
const int TROLL_SYNERGY_THRESHOLD = 2;
const float TROLL_ATTACK_SPEED_BONUS = 0.3f; // 30% attack speed bonus for 2+ trolls
const int ELF_SYNERGY_THRESHOLD = 3;
const float ELF_DODGE_CHANCE = 0.25f; // 25% dodge chance for 3+ elves
const int KNIGHT_SYNERGY_THRESHOLD = 2;
const float KNIGHT_DAMAGE_REDUCTION = 0.15f; // 15% damage reduction for 2+ humans
const int MAGE_SYNERGY_THRESHOLD = 2;
const float MAGE_DAMAGE_BOOST = 0.25f; // 25% damage boost for 2+ mages

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

    // Process battles for active games
    time_t currentTime = time(nullptr);
    for (auto &game : activeAutoChessGames) {
        if (game.second.isActive && 
            currentTime - game.second.wasUpdated >= BATTLE_INTERVAL_SECONDS) {
            processRound(game.second);
        }
    }

    // Convert payload to null-terminated string
    char payload[mp.decoded.payload.size + 1];
    memcpy(payload, mp.decoded.payload.bytes, mp.decoded.payload.size);
    payload[mp.decoded.payload.size] = 0;

    // Handle help and games commands
    if (strcmp(payload, "help") == 0 || strcmp(payload, "games") == 0) {
        auto reply = allocReply();
        const char *msg = "Games: TicTacToe(t), Hangman(h), RockPaperScissors(r) & AutoChess(ac)\n"
                         "t: new/join/board/[1-9]\n"
                         "h: new/state/[letter]\n"
                         "r: new/join/bot/[R/P/S]\n"
                         "ac: new/join/state/buy/sell/place";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return ProcessMessage::STOP;
    }

    // Handle different game commands
    if (strncmp(payload, "ttt", 3) == 0 || strncmp(payload, "t", 1) == 0) {
        // Skip "ttt " or "t " and handle the command
        const char* command = (strncmp(payload, "ttt", 3) == 0) ? payload + 4 : payload + 2;
        return handleTicTacToeCommand(mp, command) ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
    }
    else if (strncmp(payload, "hangman", 7) == 0 || strncmp(payload, "h", 1) == 0) {
        // Skip "hangman " or "h " and handle the command
        const char* command = (strncmp(payload, "hangman", 7) == 0) ? payload + 8 : payload + 2;
        if (strlen(command) == 0) {
            // If just "hangman" or "h", start a new game
            startNewHangmanGame(mp.from);
            auto reply = allocReply();
            std::string msg = "New Hangman game started!" + getHangmanStateString(activeHangmanGames[mp.from]) + 
                            "\nGuess: h [letter]";
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return ProcessMessage::STOP;
        }
        return handleHangmanCommand(mp, command) ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
    }
    else if (strncmp(payload, "rps", 3) == 0 || strncmp(payload, "r", 1) == 0) {
        // Skip "rps " or "r " and handle the command
        const char* command = (strncmp(payload, "rps", 3) == 0) ? payload + 4 : payload + 2;
        return handleRPSCommand(mp, command) ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
    }
    else if (strncmp(payload, "ac", 2) == 0) {
        // Skip "ac " and handle the command
        const char* command = payload + 3;
        return handleAutoChessCommand(mp, command) ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
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

    // Clean up old Rock Paper Scissors games
    std::vector<uint32_t> rpsGamesToRemove;
    for (const auto &game : activeRPSGames) {
        time_t timeDiff = currentTime - game.second.wasUpdated;
        if (timeDiff > GAME_TIMEOUT_SECONDS) {
            rpsGamesToRemove.push_back(game.first);
        }
    }

    // Remove old Rock Paper Scissors games
    for (uint32_t gameId : rpsGamesToRemove) {
        cleanupRPSGame(gameId);
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

            // Send to the other player
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

bool GamesModule::handleRPSCommand(const meshtastic_MeshPacket &mp, const char *command)
{
    if (strncmp(command, "new", 3) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeRPSGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        // Start a new game
        startNewRPSGame(mp.from, 0);
        auto reply = allocReply();
        const char *msg = "New Rock Paper Scissors game started! Waiting for an opponent to join...\n"
                         "Use 'rps join' to join this game or 'rps bot' to play against a bot.";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "bot", 3) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeRPSGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        // Start a new game against bot
        startNewRPSGame(mp.from, 0, true);
        auto reply = allocReply();
        const char *msg = "New Rock Paper Scissors game started against a bot!\n"
                         "Choose Rock(R), Paper(P), or Scissors(S)";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "join", 4) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeRPSGames) {
            if (game.second.player1 == mp.from || game.second.player2 == mp.from) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        // Find an available game
        for (auto &game : activeRPSGames) {
            if (game.second.player2 == 0 && game.second.player1 != mp.from && !game.second.isBotGame) {
                game.second.player2 = mp.from;
                game.second.wasUpdated = time(nullptr);

                // Notify both players
                auto reply = allocReply();
                const char *msg = "Game started! Choose Rock(R), Paper(P), or Scissors(S)";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);

                auto reply2 = allocDataPacket();
                const char *msg2 = "Opponent joined! Choose Rock(R), Paper(P), or Scissors(S)";
                reply2->decoded.payload.size = strlen(msg2);
                memcpy(reply2->decoded.payload.bytes, msg2, reply2->decoded.payload.size);
                reply2->to = game.second.player1;
                service->sendToMesh(reply2);

                return true;
            }
        }

        // No available games found
        auto reply = allocReply();
        const char *msg = "No games available to join. Start a new game with 'rps new' or play against a bot with 'rps bot'";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strlen(command) == 1 && (command[0] == 'R' || command[0] == 'P' || command[0] == 'S')) {
        return makeRPSChoice(mp, command[0]);
    }

    return false;
}

void GamesModule::startNewRPSGame(uint32_t player1, uint32_t player2, bool isBotGame)
{
    RPSGame game;
    game.player1 = player1;
    game.player2 = player2;
    game.player1Choice = 0;
    game.player2Choice = 0;
    game.player1Ready = false;
    game.player2Ready = false;
    game.isBotGame = isBotGame;
    game.wasUpdated = time(nullptr);
    activeRPSGames[player1] = game;
}

char GamesModule::getBotChoice()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 2);
    
    const char choices[] = {'R', 'P', 'S'};
    return choices[dis(gen)];
}

bool GamesModule::makeRPSChoice(const meshtastic_MeshPacket &mp, char choice)
{
    for (auto it = activeRPSGames.begin(); it != activeRPSGames.end(); ++it) {
        auto &game = it->second;
        if (game.player1 == mp.from) {
            if (game.player1Ready) {
                auto reply = allocReply();
                const char *msg = "You've already made your choice!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
            game.player1Choice = choice;
            game.player1Ready = true;
            game.wasUpdated = time(nullptr);

            // If it's a bot game, make the bot's choice immediately
            if (game.isBotGame) {
                game.player2Choice = getBotChoice();
                game.player2Ready = true;
            }
        }
        else if (game.player2 == mp.from) {
            if (game.player2Ready) {
                auto reply = allocReply();
                const char *msg = "You've already made your choice!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
            game.player2Choice = choice;
            game.player2Ready = true;
            game.wasUpdated = time(nullptr);
        }
        else {
            continue;
        }

        // If both players have made their choices, determine the winner
        if (game.player1Ready && game.player2Ready) {
            std::string result = getRPSResult(game);
            
            // Notify both players
            auto reply = allocDataPacket();
            reply->decoded.payload.size = result.length();
            memcpy(reply->decoded.payload.bytes, result.c_str(), reply->decoded.payload.size);
            reply->to = game.player1;
            service->sendToMesh(reply);

            if (!game.isBotGame) {
                auto reply2 = allocDataPacket();
                reply2->decoded.payload.size = result.length();
                memcpy(reply2->decoded.payload.bytes, result.c_str(), reply2->decoded.payload.size);
                reply2->to = game.player2;
                service->sendToMesh(reply2);
            }

            // Remove the game
            activeRPSGames.erase(it);
        }
        else {
            // Notify the player who just made their choice
            auto reply = allocReply();
            const char *msg = "Choice recorded! Waiting for opponent...";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
        }
        return true;
    }

    return false;
}

std::string GamesModule::getRPSResult(const RPSGame &game)
{
    std::stringstream ss;
    ss << "Game Results:\n";
    ss << "Player 1 chose: " << game.player1Choice << "\n";
    if (game.isBotGame) {
        ss << "Bot chose: " << game.player2Choice << "\n\n";
    } else {
        ss << "Player 2 chose: " << game.player2Choice << "\n\n";
    }

    if (game.player1Choice == game.player2Choice) {
        ss << "It's a tie!";
    }
    else if ((game.player1Choice == 'R' && game.player2Choice == 'S') ||
             (game.player1Choice == 'P' && game.player2Choice == 'R') ||
             (game.player1Choice == 'S' && game.player2Choice == 'P')) {
        ss << "Player 1 wins!";
    }
    else {
        if (game.isBotGame) {
            ss << "Bot wins!";
        } else {
            ss << "Player 2 wins!";
        }
    }

    return ss.str();
}

void GamesModule::cleanupRPSGame(uint32_t gameId)
{
    auto &game = activeRPSGames[gameId];
    std::string msg = "Rock Paper Scissors game timed out due to inactivity.";
    
    if (game.player1 != 0) {
        auto reply = allocDataPacket();
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = game.player1;
        service->sendToMesh(reply);
    }
    
    if (game.player2 != 0) {
        auto reply = allocDataPacket();
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = game.player2;
        service->sendToMesh(reply);
    }
    
    activeRPSGames.erase(gameId);
}

// Auto Chess game implementation
bool GamesModule::handleAutoChessCommand(const meshtastic_MeshPacket &mp, const char *command)
{
    if (strncmp(command, "new", 3) == 0) {
        // Check if player already has an active game
        for (const auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                auto reply = allocReply();
                const char *msg = "You already have an active game!";
                reply->decoded.payload.size = strlen(msg);
                memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        // Start a new game
        startNewAutoChessGame(mp.from);
        auto reply = allocReply();
        std::string msg = "New Auto Chess game started! Waiting for players (2-4 players needed)\n" + 
                         getAutoChessStateString(activeAutoChessGames[mp.from].players[mp.from]);
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "status", 6) == 0) {
        // Find player's active game
        for (const auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                auto reply = allocReply();
                std::string msg = "Game Status:\n";
                msg += "Players: " + std::to_string(game.second.players.size()) + "/4\n";
                msg += "Game ID: " + std::to_string(game.first) + "\n";
                msg += "Status: " + std::string(game.second.isActive ? "Active" : "Waiting for players") + "\n";
                msg += "Round: " + std::to_string(game.second.round) + "\n";
                reply->decoded.payload.size = msg.length();
                memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        auto reply = allocReply();
        const char *msg = "You don't have an active game. Start one with 'ac new' or join one with 'ac join <game_id>'";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "join", 4) == 0) {
        // Parse game ID from command
        uint32_t gameId;
        if (sscanf(command + 5, "%u", &gameId) != 1) {
            auto reply = allocReply();
            const char *msg = "Invalid game ID. Usage: ac join <game_id>";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        if (joinAutoChessGame(mp.from, gameId)) {
            auto reply = allocReply();
            std::string msg = "Joined Auto Chess game!\n" + getAutoChessStateString(activeAutoChessGames[gameId].players[mp.from]);
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }
        else {
            auto reply = allocReply();
            const char *msg = "Failed to join game. Game might be full or doesn't exist.";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }
    }
    else if (strncmp(command, "state", 5) == 0) {
        // Find player's active game
        for (const auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                auto reply = allocReply();
                std::string msg = "Current game state:\n" + getAutoChessStateString(game.second.players.at(mp.from));
                reply->decoded.payload.size = msg.length();
                memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                reply->to = mp.from;
                service->sendToMesh(reply);
                return true;
            }
        }

        auto reply = allocReply();
        const char *msg = "You don't have an active game. Start one with 'ac new' or join one with 'ac join <game_id>'";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "buy", 3) == 0) {
        // Parse unit index from command
        int unitIndex;
        if (sscanf(command + 4, "%d", &unitIndex) != 1) {
            auto reply = allocReply();
            const char *msg = "Invalid unit index. Usage: ac buy <unit_index>";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        // Find player's active game
        for (auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                if (buyUnit(game.second.players[mp.from], unitIndex)) {
                    auto reply = allocReply();
                    std::string msg = "Unit purchased!\n" + getAutoChessStateString(game.second.players[mp.from]);
                    reply->decoded.payload.size = msg.length();
                    memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                else {
                    auto reply = allocReply();
                    const char *msg = "Failed to buy unit. Not enough gold or invalid unit index.";
                    reply->decoded.payload.size = strlen(msg);
                    memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                return true;
            }
        }

        auto reply = allocReply();
        const char *msg = "You don't have an active game.";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "sell", 4) == 0) {
        // Parse unit index from command
        int unitIndex;
        if (sscanf(command + 5, "%d", &unitIndex) != 1) {
            auto reply = allocReply();
            const char *msg = "Invalid unit index. Usage: ac sell <unit_index>";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        // Find player's active game
        for (auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                if (sellUnit(game.second.players[mp.from], unitIndex)) {
                    auto reply = allocReply();
                    std::string msg = "Unit sold!\n" + getAutoChessStateString(game.second.players[mp.from]);
                    reply->decoded.payload.size = msg.length();
                    memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                else {
                    auto reply = allocReply();
                    const char *msg = "Failed to sell unit. Invalid unit index.";
                    reply->decoded.payload.size = strlen(msg);
                    memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                return true;
            }
        }

        auto reply = allocReply();
        const char *msg = "You don't have an active game.";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }
    else if (strncmp(command, "place", 5) == 0) {
        // Parse bench and board indices from command
        int benchIndex, boardIndex;
        if (sscanf(command + 6, "%d %d", &benchIndex, &boardIndex) != 2) {
            auto reply = allocReply();
            const char *msg = "Invalid indices. Usage: ac place <bench_index> <board_index>";
            reply->decoded.payload.size = strlen(msg);
            memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
            reply->to = mp.from;
            service->sendToMesh(reply);
            return true;
        }

        // Find player's active game
        for (auto &game : activeAutoChessGames) {
            if (game.second.players.find(mp.from) != game.second.players.end()) {
                if (placeUnit(game.second.players[mp.from], benchIndex, boardIndex)) {
                    auto reply = allocReply();
                    std::string msg = "Unit placed!\n" + getAutoChessStateString(game.second.players[mp.from]);
                    reply->decoded.payload.size = msg.length();
                    memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                else {
                    auto reply = allocReply();
                    const char *msg = "Failed to place unit. Invalid indices or board is full.";
                    reply->decoded.payload.size = strlen(msg);
                    memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
                    reply->to = mp.from;
                    service->sendToMesh(reply);
                }
                return true;
            }
        }

        auto reply = allocReply();
        const char *msg = "You don't have an active game.";
        reply->decoded.payload.size = strlen(msg);
        memcpy(reply->decoded.payload.bytes, msg, reply->decoded.payload.size);
        reply->to = mp.from;
        service->sendToMesh(reply);
        return true;
    }

    return false;
}

void GamesModule::startNewAutoChessGame(uint32_t player)
{
    AutoChessGame game;
    game.round = 1;
    game.isActive = false;  // Game starts inactive until enough players join
    game.wasUpdated = time(nullptr);

    AutoChessPlayer newPlayer;
    newPlayer.playerId = player;
    newPlayer.gold = 5;  // Starting gold
    newPlayer.level = 1;
    newPlayer.experience = 0;
    newPlayer.mana = 0;
    newPlayer.wasUpdated = time(nullptr);
    
    // Initialize shop
    refreshShop(newPlayer);

    game.players[player] = newPlayer;
    activeAutoChessGames[player] = game;
}

bool GamesModule::joinAutoChessGame(uint32_t player, uint32_t gameId)
{
    auto it = activeAutoChessGames.find(gameId);
    if (it == activeAutoChessGames.end())
        return false;

    // Check if game is full (max 4 players)
    if (it->second.players.size() >= 4)
        return false;

    // Check if player is already in the game
    if (it->second.players.find(player) != it->second.players.end())
        return false;

    AutoChessPlayer newPlayer;
    newPlayer.playerId = player;
    newPlayer.gold = 5;  // Starting gold
    newPlayer.level = 1;
    newPlayer.experience = 0;
    newPlayer.mana = 0;
    newPlayer.wasUpdated = time(nullptr);
    
    // Initialize shop
    refreshShop(newPlayer);

    it->second.players[player] = newPlayer;
    it->second.wasUpdated = time(nullptr);

    // Check if we have enough players to start (2-4 players)
    if (it->second.players.size() >= 2 && !it->second.isActive) {
        it->second.isActive = true;
        // Notify all players that the game is starting
        std::string msg = "Game is starting with " + std::to_string(it->second.players.size()) + " players!";
        for (const auto &p : it->second.players) {
            auto reply = allocDataPacket();
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = p.first;
            service->sendToMesh(reply);
        }
    }

    return true;
}

void GamesModule::cleanupAutoChessGame(uint32_t gameId)
{
    auto &game = activeAutoChessGames[gameId];
    std::string msg = "Auto Chess game timed out due to inactivity.";
    
    for (const auto &player : game.players) {
        auto reply = allocDataPacket();
        reply->decoded.payload.size = msg.length();
        memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
        reply->to = player.first;
        service->sendToMesh(reply);
    }
    
    activeAutoChessGames.erase(gameId);
}

std::string GamesModule::getAutoChessStateString(const AutoChessPlayer &player)
{
    std::stringstream ss;
    ss << "Level: " << player.level << " (XP: " << player.experience << ")\n";
    ss << "Gold: " << player.gold << "\n";
    ss << "Mana: " << player.mana << "\n";
    
    // Find the game this player is in
    for (const auto &game : activeAutoChessGames) {
        if (game.second.players.find(player.playerId) != game.second.players.end()) {
            ss << "Players: " << game.second.players.size() << "/4\n";
            ss << "Status: " << (game.second.isActive ? "Game in progress" : "Waiting for players (need 2-4)") << "\n";
            break;
        }
    }
    
    ss << "\n" << getShopString(player) << "\n";
    
    ss << "Bench:\n";
    for (size_t i = 0; i < player.bench.size(); i++) {
        const auto &unit = player.bench[i];
        ss << i << ". " << unit.name << " (" << unit.race << " " << unit.class_ << ")\n"
           << "   Level: " << unit.level << ", Cost: " << unit.cost << "\n";
    }
    
    ss << "\nBoard:\n";
    for (size_t i = 0; i < player.board.size(); i++) {
        const auto &unit = player.board[i];
        ss << i << ". " << unit.name << " (" << unit.race << " " << unit.class_ << ")\n"
           << "   Level: " << unit.level << "\n";
    }
    
    return ss.str();
}

void GamesModule::refreshShop(AutoChessPlayer &player)
{
    player.shop.availableUnits.clear();
    
    // Generate 5 random units from templates
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, UNIT_TEMPLATES_COUNT - 1);
    
    for (int i = 0; i < 5; i++) {
        player.shop.availableUnits.push_back(UNIT_TEMPLATES[dis(gen)]);
    }
    
    player.shop.lastRefresh = time(nullptr);
}

std::string GamesModule::getShopString(const AutoChessPlayer &player)
{
    std::stringstream ss;
    ss << "Shop:\n";
    for (size_t i = 0; i < player.shop.availableUnits.size(); i++) {
        const auto &unit = player.shop.availableUnits[i];
        ss << i << ". " << unit.name << " (" << unit.race << " " << unit.class_ << ")\n"
           << "   Cost: " << unit.cost << " gold, Health: " << unit.health 
           << ", Damage: " << unit.damage << ", Mana: " << unit.mana << "\n";
    }
    return ss.str();
}

bool GamesModule::buyUnit(AutoChessPlayer &player, int unitIndex)
{
    if (unitIndex < 0 || unitIndex >= player.shop.availableUnits.size())
        return false;
        
    const auto &unit = player.shop.availableUnits[unitIndex];
    
    // Check if player has enough gold
    if (player.gold < unit.cost)
        return false;
        
    // Add unit to bench
    player.bench.push_back(unit);
    
    // Deduct gold
    player.gold -= unit.cost;
    
    // Remove unit from shop
    player.shop.availableUnits.erase(player.shop.availableUnits.begin() + unitIndex);
    
    player.wasUpdated = time(nullptr);
    return true;
}

bool GamesModule::sellUnit(AutoChessPlayer &player, int unitIndex)
{
    if (unitIndex < 0 || unitIndex >= player.bench.size())
        return false;

    // Add gold based on unit cost
    player.gold += player.bench[unitIndex].cost;
    
    // Remove unit from bench
    player.bench.erase(player.bench.begin() + unitIndex);
    player.wasUpdated = time(nullptr);
    return true;
}

bool GamesModule::placeUnit(AutoChessPlayer &player, int benchIndex, int boardIndex)
{
    if (benchIndex < 0 || benchIndex >= player.bench.size())
        return false;
    
    if (boardIndex < 0 || boardIndex >= 9) // 3x3 board
        return false;
    
    // Check if board position is empty
    if (boardIndex < player.board.size())
        return false;
    
    // Move unit from bench to board
    player.board.push_back(player.bench[benchIndex]);
    player.bench.erase(player.bench.begin() + benchIndex);
    player.wasUpdated = time(nullptr);
    return true;
}

void GamesModule::processRound(AutoChessGame &game)
{
    // Refresh shop for all players
    for (auto &player : game.players) {
        refreshShop(player.second);
    }
    
    // Distribute gold and mana
    distributeGold(game);
    distributeMana(game);
    
    // Process battles
    processBattles(game);
    
    game.round++;
    game.wasUpdated = time(nullptr);
}

void GamesModule::processBattles(AutoChessGame &game)
{
    // Create a vector of player IDs for random matching
    std::vector<uint32_t> playerIds;
    for (const auto &player : game.players) {
        playerIds.push_back(player.first);
    }
    
    // Shuffle players for random matching
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(playerIds.begin(), playerIds.end(), gen);
    
    // Process battles in pairs
    for (size_t i = 0; i < playerIds.size(); i += 2) {
        if (i + 1 < playerIds.size()) {
            // Battle between two players
            processBattle(game, playerIds[i], playerIds[i + 1]);
        } else {
            // Odd number of players, last player gets a bye
            auto reply = allocDataPacket();
            std::string msg = "Round " + std::to_string(game.round) + ": You received a bye this round.";
            reply->decoded.payload.size = msg.length();
            memcpy(reply->decoded.payload.bytes, msg.c_str(), reply->decoded.payload.size);
            reply->to = playerIds[i];
            service->sendToMesh(reply);
        }
    }
}

void GamesModule::processBattle(AutoChessGame &game, uint32_t player1Id, uint32_t player2Id)
{
    auto &player1 = game.players[player1Id];
    auto &player2 = game.players[player2Id];
    
    // Calculate synergies
    int player1Warriors = countWarriors(player1);
    int player2Warriors = countWarriors(player2);
    int player1Trolls = countTrolls(player1);
    int player2Trolls = countTrolls(player2);
    int player1Elves = countElves(player1);
    int player2Elves = countElves(player2);
    int player1Knights = countKnights(player1);
    int player2Knights = countKnights(player2);
    int player1Mages = countMages(player1);
    int player2Mages = countMages(player2);
    
    float player1DamageReduction = (player1Warriors >= WARRIOR_SYNERGY_THRESHOLD) ? WARRIOR_DAMAGE_REDUCTION : 0.0f;
    float player2DamageReduction = (player2Warriors >= WARRIOR_SYNERGY_THRESHOLD) ? WARRIOR_DAMAGE_REDUCTION : 0.0f;
    float player1AttackSpeed = (player1Trolls >= TROLL_SYNERGY_THRESHOLD) ? TROLL_ATTACK_SPEED_BONUS : 0.0f;
    float player2AttackSpeed = (player2Trolls >= TROLL_SYNERGY_THRESHOLD) ? TROLL_ATTACK_SPEED_BONUS : 0.0f;
    bool player1HasElfSynergy = player1Elves >= ELF_SYNERGY_THRESHOLD;
    bool player2HasElfSynergy = player2Elves >= ELF_SYNERGY_THRESHOLD;
    float player1KnightReduction = (player1Knights >= KNIGHT_SYNERGY_THRESHOLD) ? KNIGHT_DAMAGE_REDUCTION : 0.0f;
    float player2KnightReduction = (player2Knights >= KNIGHT_SYNERGY_THRESHOLD) ? KNIGHT_DAMAGE_REDUCTION : 0.0f;
    float player1MageBoost = (player1Mages >= MAGE_SYNERGY_THRESHOLD) ? MAGE_DAMAGE_BOOST : 0.0f;
    float player2MageBoost = (player2Mages >= MAGE_SYNERGY_THRESHOLD) ? MAGE_DAMAGE_BOOST : 0.0f;
    
    // Battle simulation
    std::vector<AutoChessUnit> team1 = player1.board;
    std::vector<AutoChessUnit> team2 = player2.board;
    
    int team1Health = calculateTeamHealth(team1);
    int team2Health = calculateTeamHealth(team2);
    int initialTeam1Health = team1Health;
    int initialTeam2Health = team2Health;
    
    // Simple battle: units attack in order
    while (!team1.empty() && !team2.empty()) {
        // Team 1 attacks
        for (auto &unit : team1) {
            if (!team2.empty()) {
                // Check for dodge
                if (player2HasElfSynergy && shouldDodge()) {
                    continue; // Attack dodged
                }
                
                int damage = unit.damage * (1.0f - player2DamageReduction - player2KnightReduction);
                // Apply attack speed bonus
                if (player1AttackSpeed > 0) {
                    damage = static_cast<int>(damage * (1.0f + player1AttackSpeed));
                }
                // Apply mage damage boost
                if (player1MageBoost > 0 && unit.class_ == "Mage") {
                    damage = static_cast<int>(damage * (1.0f + player1MageBoost));
                }
                team2[0].health -= damage;
                if (team2[0].health <= 0) {
                    team2.erase(team2.begin());
                }
            }
        }
        
        // Team 2 attacks
        for (auto &unit : team2) {
            if (!team1.empty()) {
                // Check for dodge
                if (player1HasElfSynergy && shouldDodge()) {
                    continue; // Attack dodged
                }
                
                int damage = unit.damage * (1.0f - player1DamageReduction - player1KnightReduction);
                // Apply attack speed bonus
                if (player2AttackSpeed > 0) {
                    damage = static_cast<int>(damage * (1.0f + player2AttackSpeed));
                }
                // Apply mage damage boost
                if (player2MageBoost > 0 && unit.class_ == "Mage") {
                    damage = static_cast<int>(damage * (1.0f + player2MageBoost));
                }
                team1[0].health -= damage;
                if (team1[0].health <= 0) {
                    team1.erase(team1.begin());
                }
            }
        }
    }
    
    // Calculate results
    bool player1Won = !team1.empty();
    int player1HealthLost = initialTeam1Health - calculateTeamHealth(team1);
    int player2HealthLost = initialTeam2Health - calculateTeamHealth(team2);
    
    // Send results to players using the new function
    sendBattleResults(player1Id, game.round, player1Won, player1HealthLost,
                     player1Warriors >= WARRIOR_SYNERGY_THRESHOLD,
                     player1Trolls >= TROLL_SYNERGY_THRESHOLD,
                     player1Elves >= ELF_SYNERGY_THRESHOLD,
                     player1Knights >= KNIGHT_SYNERGY_THRESHOLD,
                     player1Mages >= MAGE_SYNERGY_THRESHOLD);
                     
    sendBattleResults(player2Id, game.round, !player1Won, player2HealthLost,
                     player2Warriors >= WARRIOR_SYNERGY_THRESHOLD,
                     player2Trolls >= TROLL_SYNERGY_THRESHOLD,
                     player2Elves >= ELF_SYNERGY_THRESHOLD,
                     player2Knights >= KNIGHT_SYNERGY_THRESHOLD,
                     player2Mages >= MAGE_SYNERGY_THRESHOLD);
}

int GamesModule::countWarriors(const AutoChessPlayer &player)
{
    int count = 0;
    for (const auto &unit : player.board) {
        if (unit.class_ == "Warrior") {
            count++;
        }
    }
    return count;
}

int GamesModule::countTrolls(const AutoChessPlayer &player)
{
    int count = 0;
    for (const auto &unit : player.board) {
        if (unit.race == "Orc") {
            count++;
        }
    }
    return count;
}

int GamesModule::countElves(const AutoChessPlayer &player)
{
    int count = 0;
    for (const auto &unit : player.board) {
        if (unit.race == "Elf") {
            count++;
        }
    }
    return count;
}

bool GamesModule::shouldDodge()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < ELF_DODGE_CHANCE;
}

void GamesModule::distributeGold(AutoChessGame &game)
{
    for (auto &player : game.players) {
        // Base gold per round
        player.second.gold += 5;
        
        // Interest (1 gold per 10 gold saved, up to 5)
        int interest = std::min(player.second.gold / 10, 5);
        player.second.gold += interest;
        
        player.second.wasUpdated = time(nullptr);
    }
}

void GamesModule::distributeMana(AutoChessGame &game)
{
    for (auto &player : game.players) {
        // Base mana per round
        player.second.mana += 10;
        
        // Cap mana at 100
        player.second.mana = std::min(player.second.mana, 100);
        
        player.second.wasUpdated = time(nullptr);
    }
}

void GamesModule::checkLevelUp(AutoChessPlayer &player)
{
    // XP required for each level
    const int xpPerLevel[] = {0, 2, 6, 12, 20, 30, 42, 56, 72, 90};
    
    // Check if player can level up
    if (player.level < 10 && player.experience >= xpPerLevel[player.level]) {
        player.level++;
        player.experience -= xpPerLevel[player.level - 1];
        player.wasUpdated = time(nullptr);
    }
}

int GamesModule::countKnights(const AutoChessPlayer &player)
{
    int count = 0;
    for (const auto &unit : player.board) {
        if (unit.race == "Human") {
            count++;
        }
    }
    return count;
}

int GamesModule::countMages(const AutoChessPlayer &player)
{
    int count = 0;
    for (const auto &unit : player.board) {
        if (unit.class_ == "Mage") {
            count++;
        }
    }
    return count;
}

void GamesModule::sendBattleResults(uint32_t playerId, int round, bool won, int healthLost, 
                                  bool hasWarrior, bool hasTroll, bool hasElf, 
                                  bool hasKnight, bool hasMage)
{
    // First message: Basic battle results
    std::string msg1 = "Round " + std::to_string(round) + " Battle:\n";
    msg1 += won ? "Victory! " : "Defeat! ";
    msg1 += "Lost " + std::to_string(healthLost) + " health";
    
    auto reply1 = allocDataPacket();
    reply1->decoded.payload.size = msg1.length();
    memcpy(reply1->decoded.payload.bytes, msg1.c_str(), reply1->decoded.payload.size);
    reply1->to = playerId;
    service->sendToMesh(reply1);
    
    // Second message: Active synergies
    std::string msg2 = "Active synergies:\n";
    if (hasWarrior) msg2 += "Warriors (-20% dmg)\n";
    if (hasTroll) msg2 += "Trolls (+30% speed)\n";
    if (hasElf) msg2 += "Elves (25% dodge)\n";
    if (hasKnight) msg2 += "Knights (-15% dmg)\n";
    if (hasMage) msg2 += "Mages (+25% dmg)";
    
    auto reply2 = allocDataPacket();
    reply2->decoded.payload.size = msg2.length();
    memcpy(reply2->decoded.payload.bytes, msg2.c_str(), reply2->decoded.payload.size);
    reply2->to = playerId;
    service->sendToMesh(reply2);
} 