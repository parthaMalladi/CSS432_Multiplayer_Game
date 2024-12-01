#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <mutex>
#include <map>

using namespace std;

const unsigned int bufSize = 1500;  // Buffer size for incoming messages
vector<pthread_t> threads;          // To track threads
mutex threadMutex;                  // Protects access to threads vector
map<int, bool> clientReady;         // Map to track client readiness

int randomNumber;                   // Random number for the game
bool numberGenerated = false;       // Flag to ensure random number is generated once
int readyClients = 0;               // Counter for ready clients
bool gameStarted = false;           // Flag to prevent late joins
mutex gameMutex;                    // Protects game state

void* thread_server(void* ptr) {
    int* client = (int*)ptr;
    pthread_t thisThread = pthread_self(); // Get current thread ID
    char buf[bufSize];

    while (true) {
        // Read message from the client
        int bytesRead = read(*client, buf, bufSize - 1);
        if (bytesRead > 0) {
            buf[bytesRead] = '\0'; // Null-terminate the string
            cout << "Received message from client: " << buf << endl;

            if (strcmp(buf, "ready") == 0) {
                lock_guard<mutex> lock(gameMutex);
                if (gameStarted) {
                    const char* msg = "Game has already started. Cannot ready up.";
                    write(*client, msg, strlen(msg));
                } else {
                    if (clientReady[*client]) {
                        const char* msg = "You are already ready.";
                        write(*client, msg, strlen(msg));
                    } else {
                        clientReady[*client] = true;
                        readyClients++;
                        cout << "Client is ready. Total ready clients: " << readyClients << endl;

                        if (!gameStarted && readyClients == threads.size()) {
                            gameStarted = true;
                            cout << "All clients are ready. Starting the game!" << endl;
                        }
                        write(*client, buf, bytesRead); // Acknowledge ready
                    }
                }
            } else if (gameStarted) {
                write(*client, buf, bytesRead); // Echo messages after the game starts
            } else {
                const char* msg = "Game has not started yet. Please send 'ready'.";
                write(*client, msg, strlen(msg));
            }
        } else {
            cerr << "Client disconnected or failed to read." << endl;
            break;
        }
    }

    // Clean up on disconnect
    {
        lock_guard<mutex> lock(gameMutex);
        if (clientReady[*client]) {
            readyClients--;
        }
        clientReady.erase(*client);
    }

    close(*client);
    delete client;
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    int serverPort = atoi(argv[1]); // Port number from command-line arguments

    // Set up server socket
    sockaddr_in serverAddr;
    bzero((char*)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(serverPort);

    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd < 0) {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    // Reuse the socket address
    const int on = 1;
    setsockopt(serverSd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Bind and listen
    if (bind(serverSd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding socket" << endl;
        return 1;
    }
    listen(serverSd, 5);

    cout << "Server is running and waiting for connections on port " << serverPort << endl;

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int clientSd = accept(serverSd, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSd < 0) {
            cerr << "Error accepting client connection" << endl;
            continue;
        }

        {
            lock_guard<mutex> lock(gameMutex);
            if (gameStarted) {
                // Reject new clients if the game has started
                const char* msg = "Game has already started. Cannot join.";
                write(clientSd, msg, strlen(msg));
                close(clientSd);
                continue;
            }

            // Generate random number if first client
            if (!numberGenerated) {
                srand(time(nullptr));
                randomNumber = rand() % 100 + 1; // Random number between 1 and 100
                numberGenerated = true;
                cout << "Random number generated: " << randomNumber << endl;
            }
        }

        // Display client information
        cout << "Connection established with client: " << inet_ntoa(clientAddr.sin_addr) << endl;

        // Create a new thread to handle the client
        int* clientPtr = new int(clientSd);
        pthread_t threadId;
        int ret = pthread_create(&threadId, nullptr, thread_server, (void*)clientPtr);
        if (ret != 0) {
            cerr << "Error creating thread" << endl;
            close(clientSd);
            delete clientPtr;
        } else {
            pthread_detach(threadId); // Detach thread to free resources automatically
            {
                lock_guard<mutex> lock(threadMutex);
                threads.push_back(threadId);
            }
        }
    }

    close(serverSd);
    return 0;
}