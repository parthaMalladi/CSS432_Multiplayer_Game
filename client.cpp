#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <netdb.h>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <server> <port>" << endl;
        return 1;
    }

    char* serverName = argv[1];
    int serverPort = atoi(argv[2]);

    // Resolve the server hostname
    struct hostent* host = gethostbyname(serverName);
    if (!host) {
        cerr << "Error: Unable to resolve server address" << endl;
        return 1;
    }

    // Set up the server address structure
    sockaddr_in serverAddr;
    bzero((char*)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    serverAddr.sin_port = htons(serverPort);

    // Create the socket
    int clientSd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSd < 0) {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    // Connect to the server
    if (connect(clientSd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error connecting to server" << endl;
        return 1;
    }

    cout << "Connected to server. You can start typing messages." << endl;

    string msg;
    while (true) {
        cout << "Enter message (or 'exit' to quit): ";
        getline(cin, msg);

        if (msg == "exit") {
            break;
        }

        // Send the message to the server
        int writeRequest = write(clientSd, msg.c_str(), msg.size());
        if (writeRequest < 0) {
            cerr << "Error sending message to server" << endl;
            break;
        }

        // Wait for the acknowledgment
        const unsigned int bufSize = 1500;
        char buffer[bufSize];
        bzero(buffer, bufSize);  // Clear the buffer

        // Read the response from the server
        int bytesRead = read(clientSd, buffer, bufSize - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // Null-terminate the response
            cout << "Server response: " << buffer << endl;
        } else {
            cerr << "Error reading response from server" << endl;
        }
    }

    // Close the socket after communication
    close(clientSd);
    return 0;
}
