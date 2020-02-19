#include "potato.hpp"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <algorithm>
#include <sstream>

#define BACKLOG 10   // how many pending connections queue will hold

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/********************************************************************/

std::vector<std::string> parse_host_port(std::string& recv){
  std::stringstream ss(recv);
  std::string temp;
  std::vector<std::string> res;
  while(std::getline(ss,temp,':')){
    res.push_back(temp);
  }
  return res;
}
/***************************************************************/
bool Send(int sendFd, std::string toSend){
  if (send(sendFd, toSend.c_str(), strlen(toSend.c_str()), 0) == -1){ 
    std::perror("send port number");
    return false;
  }
  return true;
}
/**********************************************************/
std::string potatoSerialize(potato& p){
  std::string res;
  res += std::to_string(p.hop);
  for(size_t i = 0; i < p.players.size(); i++){
    res += ':';
    res += std::to_string(p.players[i]+1);
  }
  res += ';';
  return res;
}
/********************************************************/
bool recieve(int sockfd, int* numbytes, std::string& toGet){
  while(true){
    char temp[1];
    if((*numbytes = recv(sockfd, temp, 1, 0)) == -1) {
      std::perror("recv");
      return false;
    }
    if(temp[0]!= ';'){
      toGet += temp[0];
    }
    else{
      break;
    }
  }
  return true;
}
/*********************************************************************/
void printTrace(std::string p){
  std::stringstream ss(p);
  std::string temp;
  std::vector<std::string> trace;
  while(std::getline(ss,temp,':')){
    trace.push_back(temp);
  }
  std::cout << "Trace of Potato:" << std::endl;
  for(size_t i = 1; i < trace.size(); i++){
    std::cout << trace[i];
    if(i != trace.size()-1){
      std::cout << ", ";
    }
  }
  std::cout << std::endl;
}

/*********************************************************************/
int selectPort(std::vector<int>& sockets){
  bool isOver = false;
  int fdmax = *(std::max_element(sockets.begin(),sockets.end()));
  fd_set sockfds;
  FD_ZERO(&sockfds);
  for(size_t i = 0; i < sockets.size(); i++){
    FD_SET(sockets[i], &sockfds);
  }
  while(!isOver){
    fd_set sockfds_temp = sockfds; // copy it
    if (select(fdmax+1, &sockfds_temp, NULL, NULL, NULL) == -1) {
      std::perror("select");
      return -1;
    }
    // run through the existing connections looking for data to read
    for(int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &sockfds_temp)) {
        int numbytes = 0;
        std::string recv;
        if(!recieve(i,&numbytes,recv)){
          std::cerr << "get potato give back error" << std::endl;
          return -1;
        }
        printTrace(recv);
        isOver = true;
        break;
      }
    }
  }
  //signal stop
  for(size_t k = 0; k < sockets.size(); k++){
    if(!Send(sockets[k], "x;")){ 
      std::perror("send ending signal");
      return -1;
    } 
  }
  return 0;
}

/********************************************************************/

int main(int argc, char** argv)
{
  //potato
  if (argc != 4){
    std::cerr<<"wrong num of parameter" << std::endl;
    return EXIT_FAILURE;
  }
  
  char* PORT = argv[1];
  
  //socket setup
  int yes=1;
  
  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int tryGetAddr = getaddrinfo("0.0.0.0", PORT, &hints, &servinfo);
  
  if (tryGetAddr) {
    std::cerr << "getaddrinfo:" <<  gai_strerror(tryGetAddr) << std::endl;
    return EXIT_FAILURE;
  }

  // create socket
  int sockfd;
  struct addrinfo* p;
  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      std::perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      std::perror("setsockopt");
      return EXIT_FAILURE;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      std::perror("server: bind");
      continue;
    }
    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL){
    std::cerr << "server: failed to bind" << std::endl;
    return EXIT_FAILURE;
  }

  if(listen(sockfd, BACKLOG) == -1) {
    std::perror("listen");
    return EXIT_FAILURE;
  }

  //start game
  int new_fd = 0;
  std::vector<int> playerFds; //keep track of players' fds
  std::vector<std::string> playerPts; //keep track of player's ports
  std::vector<std::string> playerHns; //keep track of player's hostnames
  struct potato thisPotato;
  int hop = atoi(argv[3]);
  thisPotato.hop = hop;
  int numPlayer = atoi(argv[2]);
  int playerCount = 0;
  int  numbytes;  
  std::cout << "Potato Ringmaster" << std::endl;
  std::cout << "Players = " << numPlayer << std::endl;
  std::cout << "Hops = " << hop << std::endl;

  struct sockaddr_storage connector_addr;
  socklen_t sin_size;
  
  while(1) {  // main accept() loop
    if(playerCount == numPlayer){
      std::cout << "Ready to start the game, ";
      break;
    }
    sin_size = sizeof connector_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&connector_addr, &sin_size);
    if (new_fd == -1) {
      std::perror("accept");
      continue;
    }

    playerFds.push_back(new_fd);
    std::cout << "Player " << playerFds.size() << " is ready to play"<< std::endl;  
    std::string recv;
    if(!recieve(new_fd,&numbytes,recv)){
      std::cerr << "get hostname: port number" << std::endl;
      return EXIT_FAILURE;
    }

    std::vector<std::string>hostPort = parse_host_port(recv);

    //char hostName[INET_ADDRSTRLEN];
    //inet_ntop(connector_addr.ss_family, get_in_addr((struct sockaddr *)&connector_addr), hostName, sizeof hostName);
    
    playerPts.push_back(hostPort[0]);
    //std::string host_str(hostName);
    playerHns.push_back(hostPort[1]);
    playerCount ++;
  }
  
  //send port number to connect to each player
  size_t to_connect = 0;
  for(size_t i = 0; i < playerFds.size(); i++){
    to_connect = (i+1)%numPlayer;
    std::string toSend = std::to_string(i+1)+ ":" +playerHns[to_connect] + ":" +playerPts[to_connect] + ":" + std::to_string(numPlayer) + ";";
    if (!Send(playerFds[i],toSend)){ 
      std::perror("send hostName:port number"); 
    }
  }

  //wait for players to connect
  int i = 0;
  while(i < 10000000){
    i ++;
  }

  //randomly through potato
  srand((unsigned int)time(NULL)); 
  int sendClientNum = rand() % numPlayer;
  thisPotato.players.push_back(sendClientNum);
  std::string potatoStr = potatoSerialize(thisPotato);

  if(!Send(playerFds[sendClientNum], potatoStr)){ 
    std::perror("send potato");
    return EXIT_FAILURE;
  }

  std::cout << "send potato to player " << sendClientNum << std::endl;

  //waiting for one client returns the potato
  if(selectPort(playerFds) < 0){
    std::cerr << "select client error" << std::endl;
    return EXIT_FAILURE;
  }

  close(sockfd);
  return EXIT_SUCCESS;
}
