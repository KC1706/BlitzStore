#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <sys/select.h>
#include <cctype> // Required for toupper
#include <unordered_map> // For key-value store

// Function to handle communication with a single client
// void handle_client(int client_fd) {
//     // Buffer to store incoming data
//     char buffer[1024] = {0};
//     while (true) {
//         // Read data from the client socket
//         int bytes_read = read(client_fd, buffer, sizeof(buffer));
//         if (bytes_read < 0) {
//             std::cerr << "failed to read\n";
//             break; // Exit the loop if reading fails
//         }
//         if (bytes_read == 0) {
//             // Client closed the connection
//             break;
//         }
//         // Convert buffer to std::string for easier handling
//         std::string request(buffer, bytes_read);
//         // Check if the request contains "PING"
//         if (request.find("PING") != std::string::npos) {
//             // Prepare the response
//             std::string respond("+PONG\r\n");
//             // Send the response back to the client
//             write(client_fd, respond.c_str(), respond.size());
//         }
//         // Clear the buffer for the next read
//         memset(buffer, 0, sizeof(buffer));
//     }
//     // Close the client socket when done
//     close(client_fd);
// }

// Helper function: Convert string to uppercase
std::string to_upper(const std::string& s) {
    std::string result = s;
    for (char& c : result) c = toupper(c);
    return result;
}

// Minimal RESP parser: parses a buffer into a vector of strings (command and args)
// Returns empty vector if parsing fails or incomplete
std::vector<std::string> parse_resp_command(const std::string& buffer) {
    std::vector<std::string> result;
    size_t pos = 0;
    if (buffer[pos] != '*') return result; // Not an array
    pos++;
    // Parse array length
    int array_len = 0;
    while (pos < buffer.size() && isdigit(buffer[pos])) {
        array_len = array_len * 10 + (buffer[pos] - '0');
        pos++;
    }
    if (pos + 1 >= buffer.size() || buffer[pos] != '\r' || buffer[pos+1] != '\n') return result;
    pos += 2;
    for (int i = 0; i < array_len; ++i) {
        if (pos >= buffer.size() || buffer[pos] != '$') return result;
        pos++;
        // Parse bulk string length
        int str_len = 0;
        while (pos < buffer.size() && isdigit(buffer[pos])) {
            str_len = str_len * 10 + (buffer[pos] - '0');
            pos++;
        }
        if (pos + 1 >= buffer.size() || buffer[pos] != '\r' || buffer[pos+1] != '\n') return result;
        pos += 2;
        // Extract the string
        if (pos + str_len > buffer.size()) return result;
        std::string arg = buffer.substr(pos, str_len);
        result.push_back(arg);
        pos += str_len;
        if (pos + 1 >= buffer.size() || buffer[pos] != '\r' || buffer[pos+1] != '\n') return result;
        pos += 2;
    }
    return result;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // Create a TCP socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Set SO_REUSEADDR to avoid 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  // Configure server address structure
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  // Bind the socket to port 6379
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  // Start listening for incoming connections
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  std::cout << "Waiting for a client to connect...\n";

  // Debug log
  std::cout << "Logs from your program will appear here!\n";


  //using threads

  // Main server loop: accept and handle clients concurrently
//   while (true) {
//     // Accept a new client connection (blocking call)
//     int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
//     if (client_fd < 0) {
//       std::cerr << "Failed to accept client\n";
//       continue; // Try accepting the next client
//     }
//     std::cout << "Client connected\n";
//     // Spawn a new thread to handle the client
//     std::thread t(handle_client, client_fd);
//     t.detach(); // Detach the thread so it cleans up after itself
//   }
//   // Close the server socket (unreachable in this loop, but good practice)
//   close(server_fd);
//   return 0;
// }



//using event loop

 // 5. Track all active client sockets
 std::vector<int> client_fds;
 // Key-value store for SET/GET
 std::unordered_map<std::string, std::string> kv_store;

 // 6. Main event loop
while (true) {
  fd_set read_fds;
  FD_ZERO(&read_fds);

  // Add the server socket (for new connections)
  FD_SET(server_fd, &read_fds);
  int max_fd = server_fd;

  // Add all client sockets
  for (int fd : client_fds) {
      FD_SET(fd, &read_fds);
      if (fd > max_fd) max_fd = fd;
  }

  // 7. Wait for activity on any socket
  int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
  if (activity < 0) {
      std::cerr << "select error\n";
      break;
  }

  // 8. Check if there is a new connection
  if (FD_ISSET(server_fd, &read_fds)) {
      struct sockaddr_in client_addr;
      socklen_t client_addr_len = sizeof(client_addr);
      int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
      if (client_fd < 0) {
          std::cerr << "Failed to accept client\n";
      } else {
          std::cout << "Client connected\n";
          client_fds.push_back(client_fd);
      }
  }
    // 9. Check all clients for incoming data
    std::vector<int> disconnected_fds;
    for (int fd : client_fds) {
        if (FD_ISSET(fd, &read_fds)) {
            char buffer[1024] = {0};
            int bytes_read = read(fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                // Client disconnected or error
                std::cout << "Client disconnected\n";
                close(fd);
                disconnected_fds.push_back(fd);
            } else {
                std::string request(buffer, bytes_read);
                // Try to parse as RESP command
                std::vector<std::string> parts = parse_resp_command(request);
                if (!parts.empty()) {
                    std::string command = to_upper(parts[0]);
                    if (command == "PING") {
                        std::string respond = "+PONG\r\n";
                        write(fd, respond.c_str(), respond.size());
                    } else if (command == "ECHO" && parts.size() >= 2) {
                        // RESP bulk string: $<len>\r\n<arg>\r\n
                        std::string arg = parts[1];
                        std::string respond = "$" + std::to_string(arg.size()) + "\r\n" + arg + "\r\n";
                        write(fd, respond.c_str(), respond.size());
                    } else if(command == "SET" && parts.size() >= 3){
                        std::string key = parts[1];
                        std::string value = parts[2];
                        kv_store[key] = value;
                        std::string respond = "+OK\r\n";
                        write(fd, respond.c_str(), respond.size());
                    } else if(command == "GET" && parts.size() >= 2){
                        std::string key = parts[1];
                        auto it = kv_store.find(key);
                        if (it != kv_store.end()) {
                            const std::string& value = it->second;
                            std::string respond = "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
                            write(fd, respond.c_str(), respond.size());
                        } else {
                            std::string respond = "$-1\r\n";
                            write(fd, respond.c_str(), respond.size());
                        }
                    }
                    else {
                        // Unknown command, send error
                        std::string respond = "-ERR unknown command\r\n";
                        write(fd, respond.c_str(), respond.size());
                    }
                } else {
                    // Fallback: check for inline PING (for old test)
                    if (request.find("PING") != std::string::npos) {
                        std::string respond = "+PONG\r\n";
                        write(fd, respond.c_str(), respond.size());
                    }
                }
            }
        }
    }

    // 10. Remove disconnected clients from the list
    for (int fd : disconnected_fds) {
        client_fds.erase(std::remove(client_fds.begin(), client_fds.end(), fd), client_fds.end());
    }
}

// 11. Cleanup
for (int fd : client_fds) close(fd);
close(server_fd);
return 0;
}
