#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>

using namespace std;

const unsigned int bufSize = 1500; // Buffer size for incoming messages

// Thread function to handle client requests
void* thread_server(void* ptr) {
    int* client = (int*)ptr;
    char buf[bufSize];

    while (true) {
        // Read message from the client
        int bytesRead = read(*client, buf, bufSize - 1);
        if (bytesRead > 0) {
            buf[bytesRead] = '\0'; // Null-terminate the string
            cout << "Received message from client: " << buf << endl;

            // Send acknowledgment back to the client
            write(*client, buf, bytesRead);
        } else {
            cerr << "Failed to read from client or client disconnected." << endl;
            break; // Exit loop if client disconnects
        }
    }

    // Clean up
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
        }
    }

    close(serverSd);
    return 0;
}
