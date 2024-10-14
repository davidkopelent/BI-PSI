#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cmath>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>
#include <vector>
#include <queue>
#include <cctype>
#include <algorithm>
using namespace std;

#define BUFFER_SIZE 102
#define TIMEOUT 1
#define TIMEOUT_RECHARGING 5

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

#define SERVER_CONFIRMATION 0
#define SERVER_KEY_REQUEST "107 KEY REQUEST\a\b"
#define SERVER_SYNTAX_ERROR "301 SYNTAX ERROR\a\b"
#define SERVER_LOGIN_FAILED "300 LOGIN FAILED\a\b"
#define SERVER_OK "200 OK\a\b"
#define SERVER_MOVE "102 MOVE\a\b"
#define SERVER_TURN_LEFT "103 TURN LEFT\a\b"
#define SERVER_TURN_RIGHT "104 TURN RIGHT\a\b"
#define SERVER_PICK_UP "105 GET MESSAGE\a\b"
#define SERVER_LOGOUT "106 LOGOUT\a\b"
#define SERVER_LOGIC_ERROR "302 LOGIC ERROR\a\b"
#define SERVER_KEY_OUT_OF_RANGE_ERROR "303 KEY OUT OF RANGE\a\b"

#define CLIENT_RECHARGING "RECHARGING\a\b"
#define CLIENT_FULL_POWER "FULL POWER\a\b"

const int keyServer[5]{23019, 32037, 18789, 16443, 18189};
const int keyClient[5]{32037, 29295, 13603, 29533, 21952};

/*-------------------------------------------------------------------------*/

class InputCheck
{
public:
    bool controller(int &clnt, int state, string message);
    bool checkDigits(string message);
};

/*-------------------------------------------------------------------------*/

class Coordinates
{
public:
    int x;
    int y;
    int orientation;
    Coordinates(void) = default;
    Coordinates(int xn, int yn);
    int arrayToInteger(int array[], int size);
    bool getCoordinates(string message, int length);
    bool checkMiddle(void);
    int setOrientation(const Coordinates &a, const Coordinates &b);
    bool operator==(const Coordinates &a);
};

/*-------------------------------------------------------------------------*/

Coordinates::Coordinates(int xn, int yn) : x(xn),
                                           y(yn) {}

/*-------------------------------------------------------------------------*/

int Coordinates::arrayToInteger(int array[], int size)
{
    int result = 0, base = pow(10, size - 1);
    for (int i = 0; i < size; i++)
    {
        result = result + array[i] * base;
        base /= 10;
    }
    return result;
}

/*-------------------------------------------------------------------------*/

bool Coordinates::getCoordinates(string message, int length)
{
    int firstNumber[5], secondNumber[5];
    int sizeA = 0, sizeB = 0, signA = 0, signB = 0, i = 0;
    bool digit = true;

    for (i = 0; i < length; i++)
    {
        if (isdigit(message[i]))
        {
            firstNumber[sizeA++] = message[i] - '0';
            if (signA == 0)
                signA = (message[i - 1] == '-') ? -1 : 1;
            digit = false;
        }
        else if (!digit)
        {
            if (message[i] == '.')
                return false;
            break;
        }
    }

    digit = true;
    for (int j = i; j < length; j++)
    {
        if (isdigit(message[j]))
        {
            secondNumber[sizeB++] = message[j] - '0';
            if (signB == 0)
                signB = (message[j - 1] == '-') ? -1 : 1;
            digit = false;
        }
        else if (!digit)
        {
            if (message[j] == '.' || isspace(message[i]))
                return false;
            break;
        }
    }

    x = signA * arrayToInteger(firstNumber, sizeA);
    y = signB * arrayToInteger(secondNumber, sizeB);
    return true;
}

/*-------------------------------------------------------------------------*/

bool Coordinates::checkMiddle(void)
{
    return (x == 0 && y == 0);
}

/*-------------------------------------------------------------------------*/

int Coordinates::setOrientation(const Coordinates &first, const Coordinates &second)
{
    if (second.x > first.x)
    {
        return EAST;
    }
    else if (second.x < first.x)
    {
        return WEST;
    }
    else if (second.y > first.y)
    {
        return NORTH;
    }
    else if (second.y < first.y)
    {
        return SOUTH;
    }
    return -10;
}

/*-------------------------------------------------------------------------*/

bool Coordinates::operator==(const Coordinates &coord)
{
    return (x == coord.x && y == coord.y);
}

/*-------------------------------------------------------------------------*/

class Server
{
private:
    bool recharge(int clnt, fd_set sockets);
    void createHashServer(string message, int length);
    void createHashClient(string message, int length);
    bool sendKeyRequest(int &clnt);
    bool sendKey(int &clnt);
    bool sendKeyConfirm(int &clnt);
    bool readCoordinates(int &clnt, fd_set &sockets, Coordinates &cords);
    bool isFinish(int &clnt, fd_set &sockets);
    bool initialMove(int &clnt, fd_set &sockets);
    int closest(int required, int current);
    bool moveCloserToFinish(int &clnt, fd_set &sockets);
    int turn(void);
    bool overtakeCheck(int axis);
    bool overtake(int &clnt, fd_set &sockets);
    bool navigate(int &clnt, fd_set &sockets);

public:
    int state;
    int serverHash;
    string serverHashed;
    int clientHash;
    string clientHashed;
    queue<string> messages;
    Coordinates coords;
    Coordinates prev;
    bool recharging;

    Server(void) = default;
    bool readMessage(int &clnt, fd_set &sockets, bool recharger = false);
    bool sendMessage(int &clnt, const char message[], const int length);
    bool controller(int &clnt, fd_set &sockets);
};

/*-------------------------------------------------------------------------*/

void Server::createHashServer(string message, int length)
{
    serverHash = 0;
    for (int i = 0; i < length; i++)
        serverHash += (unsigned int)message[i];
    serverHash = (serverHash * 1000) % 65536;
}

/*-------------------------------------------------------------------------*/

void Server::createHashClient(string message, int length)
{
    clientHash = 0;
    for (int i = 0; i < length; i++)
        clientHash += (unsigned int)message[i];
    clientHash = (clientHash * 1000) % 65536;
}

/*-------------------------------------------------------------------------*/

bool Server::sendKeyRequest(int &clnt)
{
    string message = messages.front();
    createHashServer(message, message.length());
    createHashClient(message, message.length());
    sendMessage(clnt, SERVER_KEY_REQUEST, sizeof(SERVER_KEY_REQUEST));
    messages.pop();
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::sendKey(int &clnt)
{
    string message = messages.front();

    serverHash = (serverHash + keyServer[message[0] - '0']) % 65536;
    serverHashed = to_string(serverHash) + "\a\b";

    clientHash = (clientHash + keyClient[message[0] - '0']) % 65536;
    clientHashed = to_string(clientHash) + "\a\b";

    sendMessage(clnt, serverHashed.c_str(), serverHashed.length() + 1);
    messages.pop();
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::sendKeyConfirm(int &clnt)
{
    string message = messages.front();

    if (strncmp(clientHashed.c_str(), message.c_str(), message.length()) == 0)
    {
        sendMessage(clnt, SERVER_OK, sizeof(SERVER_OK));
    }
    else
    {
        sendMessage(clnt, SERVER_LOGIN_FAILED, sizeof(SERVER_LOGIN_FAILED));
        close(clnt);
        return false;
    }

    messages.pop();
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::readCoordinates(int &clnt, fd_set &sockets, Coordinates &cords)
{
    if (!readMessage(clnt, sockets))
        return false;

    string tmp = messages.front();
    if (!cords.getCoordinates(tmp, tmp.length()))
        return false;

    messages.pop();
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::isFinish(int &clnt, fd_set &sockets)
{
    if (coords.checkMiddle())
    {
        sendMessage(clnt, SERVER_PICK_UP, sizeof(SERVER_PICK_UP));
        if (!readMessage(clnt, sockets))
            return false;
        sendMessage(clnt, SERVER_LOGOUT, sizeof(SERVER_LOGOUT));
        close(clnt);
        return true;
    }
    return false;
}

/*-------------------------------------------------------------------------*/

bool Server::initialMove(int &clnt, fd_set &sockets)
{
    Coordinates first, second;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
    if (!readCoordinates(clnt, sockets, first))
        return false;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
    if (!readCoordinates(clnt, sockets, second))
        return false;

    if (first == second)
    {
        sendMessage(clnt, SERVER_TURN_LEFT, sizeof(SERVER_TURN_LEFT));
        readCoordinates(clnt, sockets, first);
        sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
        readCoordinates(clnt, sockets, second);
    }

    coords.orientation = coords.setOrientation(first, second);
    coords.x = second.x;
    coords.y = second.y;
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::overtakeCheck(int axis)
{
    return ((coords.y == 0 && axis == 0) || (coords.x == 0 && axis == 1));
}

/*-------------------------------------------------------------------------*/

bool Server::overtake(int &clnt, fd_set &sockets)
{
    int axis = 0;
    if (coords.y == 0)
        axis = 1;

    if (coords.x == 0)
        axis = 2;

    sendMessage(clnt, SERVER_TURN_LEFT, sizeof(SERVER_TURN_LEFT));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    coords.orientation = (coords.orientation - 1) % 4;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    if (overtakeCheck(axis))
        return true;

    sendMessage(clnt, SERVER_TURN_RIGHT, sizeof(SERVER_TURN_RIGHT));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    coords.orientation = (coords.orientation + 1) % 4;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    if (overtakeCheck(axis))
        return true;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));

    if (!readCoordinates(clnt, sockets, coords))
        return false;

    if (overtakeCheck(axis))
        return true;

    sendMessage(clnt, SERVER_TURN_RIGHT, sizeof(SERVER_TURN_RIGHT));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    coords.orientation = (coords.orientation + 1) % 4;

    sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    sendMessage(clnt, SERVER_TURN_LEFT, sizeof(SERVER_TURN_LEFT));
    if (!readCoordinates(clnt, sockets, coords))
        return false;

    coords.orientation = (coords.orientation - 1) % 4;
    return true;
}

/*-------------------------------------------------------------------------*/

int Server::closest(int required, int current)
{
    int left = 0;
    int right = 0;
    int directions[] = {NORTH, EAST, SOUTH, WEST};

    if (required == current)
        return 0;

    int index = required;
    while (directions[index] != required)
    {
        index = (index + 1) % 4;
        right++;
    }

    index = required;
    while (directions[index] != required)
    {
        index = (index - 1) % 4;
        left++;
    }

    if (left == right || right < left)
        return 1;
    return -1;
}

/*-------------------------------------------------------------------------*/

bool Server::moveCloserToFinish(int &clnt, fd_set &sockets)
{
    while (true)
    {
        int closer = turn();
        if (closer == 0)
            break;

        if (closer == -1)
        {
            sendMessage(clnt, SERVER_TURN_LEFT, sizeof(SERVER_TURN_LEFT));
        }
        else
        {
            sendMessage(clnt, SERVER_TURN_RIGHT, sizeof(SERVER_TURN_RIGHT));
        }

        if (!readCoordinates(clnt, sockets, coords))
            return false;
    }
    return true;
}

/*-------------------------------------------------------------------------*/

int Server::turn(void)
{
    int closer = 0;

    if (coords.y > 0)
    {
        closer = closest(SOUTH, coords.orientation);
    }
    else if (coords.y < 0)
    {
        closer = closest(NORTH, coords.orientation);
    }
    else if (coords.x > 0)
    {
        closer = closest(WEST, coords.orientation);
    }
    else if (coords.x < 0)
    {
        closer = closest(EAST, coords.orientation);
    }

    coords.orientation = (coords.orientation + closer) % 4;
    return closer;
}

/*-------------------------------------------------------------------------*/

bool Server::navigate(int &clnt, fd_set &sockets)
{
    int currentState = 1;

    if (coords.checkMiddle())
        return true;

    if (coords.y == 0)
        currentState = 3;

    if (!moveCloserToFinish(clnt, sockets))
        return false;

    if (currentState == 1)
    {
        while (coords.y != 0)
        {
            int prevX = coords.x;
            int prevY = coords.y;

            sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
            if (!readCoordinates(clnt, sockets, coords))
                return false;

            if (prevX == coords.x && prevY == coords.y)
            {
                if (!overtake(clnt, sockets))
                    return false;
            }
        }

        if (!moveCloserToFinish(clnt, sockets))
            return false;
    }

    while (coords.x != 0)
    {
        int prevX = coords.x;
        int prevY = coords.y;

        sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));
        if (!readCoordinates(clnt, sockets, coords))
            return false;

        if (prevX == coords.x && prevY == coords.y)
        {
            if (!overtake(clnt, sockets))
                return false;
        }
    }

    if (!moveCloserToFinish(clnt, sockets))
        return false;

    while (coords.y != 0)
    {
        int prevX = coords.x;
        int prevY = coords.y;
        sendMessage(clnt, SERVER_MOVE, sizeof(SERVER_MOVE));

        if (!readCoordinates(clnt, sockets, coords))
            return false;

        if (prevX == coords.x && prevY == coords.y)
        {
            if (!overtake(clnt, sockets))
                return false;
        }
    }
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::sendMessage(int &clnt, const char message[], const int length)
{
    if (send(clnt, message, length - 1, 0) < 0)
        return false;
    return true;
}

/*-------------------------------------------------------------------------*/

bool Server::recharge(int clnt, fd_set sockets)
{
    recharging = true;
    if (!readMessage(clnt, sockets))
        return false;

    string message = messages.front();
    if (message == "FULL POWER")
    {
        messages.pop();
        if (messages.empty())
            readMessage(clnt, sockets);
        return true;
    }
    return false;
}

/*-------------------------------------------------------------------------*/

bool Server::readMessage(int &clnt, fd_set &sockets, bool recharger)
{
    InputCheck check;
    string msg;
    int maxLengths[] = {18, 3, 7, 11, 98, 98, 12};
    struct timeval timeout;

    if (!messages.empty())
        return true;

    int bytesReceived = 0;
    char buffer[BUFFER_SIZE];

    while (true)
    {
        timeout.tv_sec = (recharging == true) ? 4 : TIMEOUT;
        timeout.tv_usec = 500;
        FD_ZERO(&sockets);
        FD_SET(clnt, &sockets);

        if (select(clnt + 1, &sockets, NULL, NULL, &timeout) < 0)
        {
            close(clnt);
            return false;
        }

        if (!FD_ISSET(clnt, &sockets))
        {
            cout << "Timeout!" << endl;
            close(clnt);
            return false;
        }

        bytesReceived = recv(clnt, buffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived <= 0)
        {
            cout << "Recv error" << endl;
            close(clnt);
            return false;
        }

        string msg1;
        for (int i = 0; i < bytesReceived; i++)
        {
            msg1.push_back(buffer[i]);
            if (buffer[i] == '\b' && msg1[msg1.length() - 2] == '\a')
            {
                msg1.pop_back();
                msg1.pop_back();
            }
        }

        if (state == 2 && msg1 == "FULL POWER")
        {
            if (messages.empty())
                readMessage(clnt, sockets);
            return true;
        }
        else if (msg1 == "RECHARGING")
        {
            recharging = true;
            if (state == 6)
            {
                sendMessage(clnt, SERVER_LOGIC_ERROR, sizeof(SERVER_LOGIC_ERROR));
                close(clnt);
                return false;
            }

            if (!recharge(clnt, sockets))
                return false;
            return true;
        }
        else
        {
            recharging = false;
        }

        for (int i = 0; i < bytesReceived; i++)
        {
            msg.push_back(buffer[i]);
            if (i > maxLengths[state - 1] && ((int)msg.size()) == i + 1)
            {
                sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
                close(clnt);
                return false;
            }

            if (buffer[i] == '\b' && (msg[msg.length() - 2] == '\a'))
            {
                msg.pop_back();
                msg.pop_back();

                if (state == 2 && msg1 == "FULL POWER")
                {
                    if (messages.empty())
                        readMessage(clnt, sockets);
                    return true;
                }
                else if (msg == "RECHARGING")
                {
                    recharging = true;
                    if (state == 6)
                    {
                        sendMessage(clnt, SERVER_LOGIC_ERROR, sizeof(SERVER_LOGIC_ERROR));
                        close(clnt);
                        return false;
                    }

                    if (!recharge(clnt, sockets))
                        return false;
                    return true;
                }
                else
                {
                    recharging = false;
                }

                if (!check.controller(clnt, state, msg))
                {
                    close(clnt);
                    return false;
                }

                messages.push(msg);

                if (i == bytesReceived - 1)
                    return true;
                msg.clear();
            }
        }
    }
    return false;
}

/*-------------------------------------------------------------------------*/

bool Server::controller(int &clnt, fd_set &sockets)
{
    switch (state)
    {
    case 1:
        if (!sendKeyRequest(clnt))
            return false;
        break;
    case 2:
        if (!sendKey(clnt))
            return false;
        break;
    case 3:
        if (!sendKeyConfirm(clnt))
            return false;
        break;
    case 4:
        if (!initialMove(clnt, sockets))
        {
            sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            close(clnt);
            return false;
        }
        break;
    case 5:
        if (!navigate(clnt, sockets))
        {
            sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            close(clnt);
            return false;
        }
        break;
    case 6:
        if (!isFinish(clnt, sockets))
        {
            sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            close(clnt);
            return false;
        }
        break;
    }
    return true;
}

/*-------------------------------------------------------------------------*/

bool InputCheck::controller(int &clnt, int state, string message)
{
    Server server;
    switch (state)
    {
    case 1:
        if (message.length() >= 20)
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }
        break;
    case 2:
        if (!isdigit(message[0]))
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }

        if (message[0] - '0' < 0 || message[0] - '0' > 4)
        {
            server.sendMessage(clnt, SERVER_KEY_OUT_OF_RANGE_ERROR, sizeof(SERVER_KEY_OUT_OF_RANGE_ERROR));
            return false;
        }
        break;
    case 3:
        if (!checkDigits(message) || message.length() > 5)
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }
        break;
    case 4:
        if (message.length() >= 12)
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }
        break;
    case 5:
        if (message.length() >= 12)
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }
        break;
    case 6:
        if (message.length() > 100)
        {
            server.sendMessage(clnt, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR));
            return false;
        }
        break;
    }

    return true;
}

/*-------------------------------------------------------------------------*/

bool InputCheck::checkDigits(string message)
{
    return all_of(message.begin(), message.end(), ::isdigit);
}

/*-------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        cerr << "Usage: server port" << endl;
        return EXIT_FAILURE;
    }

    int sckt = socket(AF_INET, SOCK_STREAM, 0);
    if (sckt < 0)
    {
        perror("Cannot create socket: ");
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port == 0)
    {
        cerr << "Usage: server port" << endl;
        close(sckt);
        return EXIT_FAILURE;
    }

    Server server;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sckt, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Error bind(): ");
        close(sckt);
        return EXIT_FAILURE;
    }

    if (listen(sckt, 10) < 0)
    {
        perror("Error listen()!");
        close(sckt);
        return EXIT_FAILURE;
    }

    struct sockaddr_in remoteAddress;
    socklen_t size = 0;
    while (true)
    {
        int clnt = accept(sckt, (struct sockaddr *)&remoteAddress, &size);
        if (clnt < 0)
        {
            perror("Error accept()!");
            close(sckt);
            return EXIT_FAILURE;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            close(sckt);
            fd_set sockets;

            //  Stage 1 - Authetication
            // ----------------------------------------------------------------------------------------
            server.state = 1;
            server.readMessage(clnt, sockets);
            server.controller(clnt, sockets);

            //  Stage 2 - Server Code
            // ----------------------------------------------------------------------------------------
            server.state = 2;
            server.readMessage(clnt, sockets);
            server.controller(clnt, sockets);

            //  Stage 3 - Client Code
            // ----------------------------------------------------------------------------------------
            server.state = 3;
            server.readMessage(clnt, sockets);
            server.controller(clnt, sockets);

            //  Stage 4 - First and Second move
            // ----------------------------------------------------------------------------------------
            server.state = 4;
            server.controller(clnt, sockets);

            //  Stage 5 - Movement
            // ----------------------------------------------------------------------------------------
            server.state = 5;
            server.controller(clnt, sockets);

            //  Stage 6 - Pick up and logout
            // ----------------------------------------------------------------------------------------
            server.state = 6;
            server.controller(clnt, sockets);

            close(clnt);
            return EXIT_SUCCESS;
        }

        int status = 0;
        waitpid(0, &status, WNOHANG);
        close(clnt);
    }

    close(sckt);
    return EXIT_SUCCESS;
}
