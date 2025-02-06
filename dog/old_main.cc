#include <sys/socket.h> // socket stuff
#include <netinet/in.h> // internet addresses n stuff (ipv4, etc)
#include <arpa/inet.h> // manipulating ip addresses
#include <string>
#include <iostream>

int main(int argc, char const* argv[])
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000); // changes to network byte order
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    std::string request = "GET / HTTP/1.1\r\n";
    request += "Host: example.com\r\n";
    request += "Connection: close\r\n\r\n";
    send(sock_fd, request.c_str(), request.length(), 0);

    char buffer[4096];
    std::string response;
    int bytes_received;
    while ((bytes_received = recv(sock_fd, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes_received);
    }

    std::cout << response << std::endl;

    return 0;
}
