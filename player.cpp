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
#include <sstream>
#include <vector>
#include <algorithm>

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/***************************************************************/

int acceptSocket(int sockfd){
  struct sockaddr_storage connector_addr;
  socklen_t sin_size;
  sin_size = sizeof connector_addr;
  int new_fd = accept(sockfd, (struct sockaddr *)&connector_addr, &sin_size);
  if (new_fd == -1) {
    std::perror("accept");
    return -1;
  }
  return new_fd;
}

/**************************************************************/
int connectToSocket(const char* hostname, const char* port, int& socket_fd){
  struct addrinfo host_info;
  struct addrinfo *host_info_list;

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;

  // std::cout << hostname << ":" << port << std::endl;
  
  int status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    std::cerr << gai_strerror(status) << std::endl;
    std::cerr << "Error: cannot get address info for host" << std::endl;
    std::cerr << "(" << hostname << "," << port << ")" << std::endl;
    return -1;
  } 
  
  socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    std::perror("build socket");
    std::cerr << "Error: cannot create socket" << std::endl;
    std::cerr << "  (" << hostname << ":" << port << ")" << std::endl;
    return -1;
  }
  
  status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    std::perror("connect client");
    std::cerr << "Error: cannot connect to socket" << std::endl;
    std::cerr << "  (" << hostname << ":" << port << ")" << std::endl;
    return -1;
  }
  return 0;
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
  //std::cout << "client: received " << toGet << std::endl;
  return true;
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
/**********************************************************/

int setupSocket(const char* port, int sockfd, int& socket_fd_own){
  int status;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = "0.0.0.0";
  //const char *hostname = NULL;

  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags    = AI_PASSIVE;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  
  if (status != 0) {
    std::cerr << "Error: cannot get address info for host" << std::endl;
    std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
    return -1;
  }
  
  socket_fd_own = socket(host_info_list->ai_family, 
                         host_info_list->ai_socktype, 
                         host_info_list->ai_protocol);
  if (socket_fd_own == -1) {
    std::cerr << "Error: cannot create socket" << std::endl;
    std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
    return -1;
  }

  int yes = 1;
  status = setsockopt(socket_fd_own, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd_own, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    std::cerr << "Error: cannot bind socket" << std::endl;
    std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
    return -1;
  }

  status = listen(socket_fd_own, 100);
  if (status == -1) {
    std::cerr << "Error: cannot listen on socket" << std::endl; 
    std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
    return -1;
  }
 
  char myIP[16];
  unsigned int myPort;
  struct sockaddr_in my_addr;
  bzero(&my_addr, sizeof(my_addr));
  socklen_t len = sizeof(my_addr);
  getsockname(socket_fd_own, (struct sockaddr *) &my_addr, &len);
  inet_ntop(AF_INET, &my_addr.sin_addr, myIP, sizeof(myIP));
  myPort = ntohs(my_addr.sin_port);

  //std::cout << myPort << std::endl;
  //std::string portStr = std::to_string(myPort) + ':' + myIP +';';
  std::string portStr = std::to_string(myPort) + ";";
  std::cout << portStr << std::endl;
  char const *pchar = portStr.c_str(); 
  
  if (send(sockfd, pchar, strlen(pchar), 0) == -1){ 
    std::perror("send port number"); 
  }
  
  return 0;
}
/**********************************************************/
std::string potatoSerialize(potato& p){
  std::string res;
  res += std::to_string(p.hop);
  for(size_t i = 0; i < p.players.size(); i++){
    res += ':';
    res += std::to_string(p.players[i]);
  }
  res += ';';
  return res;
}
/********************************************************/
void potatoDeserialize(struct potato& p, std::string pStr){
  std::vector<std::string> pInfo = parse_host_port(pStr);
  p.hop = std::stoi(pInfo[0]);
  for(size_t i = 1; i < pInfo.size(); i++){
    p.players.push_back(std::stoi(pInfo[i]));
  }
}
/***********************************************************/

void sendEnd(std::vector<int>& sockets, int num){
  if (send(sockets[num],"x;", strlen("x;"), 0) == -1){
      std::perror("send potato to other client");
    }
}

/***********************************************************/
int getMsg(struct potato& p, int socketfd){
  int numbytes;
  std::string recv;
  if(!recieve(socketfd,&numbytes,recv)){
    std::cerr << "get potato string" << std::endl;
    return -1;
  }
  else if(recv[0] == 'x'){
    return 1;
  }
  else{
    potatoDeserialize(p,recv);
    return 0;
  }
}

/**********************************************************/
int handlePotato(struct potato& p, std::vector<int>& sockets, int ownNum, int playerNum){
  std::cout << "I got the potato!";
  p.hop -= 1;
  if(p.hop == 0){
    std::cout << "I'm 'IT'!" << std::endl;
    std::string sendback = potatoSerialize(p);
    if (send(sockets[0],sendback.c_str(), strlen(sendback.c_str()), 0) == -1){ 
      std::perror("send potato back to master");
      return -1;
    }
    else{
      return 1;
    }
  }
  else{
    p.players.push_back(ownNum);
    std::string potatoStr = potatoSerialize(p);
    
    srand(time(NULL)+ownNum+p.hop*29);
    
    int newSendClientNum = rand()%2+1;
    int sendNum = 0;
    if(newSendClientNum == 1){ //send to next
      sendNum = ownNum % playerNum + 1;
      std::cout << " Send Potato to Player " << sendNum << "!" << std::endl;
    }
    else{ //send to prev
      sendNum = (ownNum - 1 != 0) ? ownNum-1 : playerNum;
      std::cout << " Send Potato to Player " << sendNum << "!" << std::endl;
    }
    if (send(sockets[newSendClientNum],potatoStr.c_str(), strlen(potatoStr.c_str()), 0) == -1){
      std::perror("send potato to other client");
      return -1;
    }
  }
  return 0;
}

/**********************************************************/
int selectPort(std::vector<int>& sockets, int ownNum, int playerNum){
  int fdmax = *(std::max_element(sockets.begin(),sockets.end()));
  fd_set sockfds;
  FD_ZERO(&sockfds);
  for(size_t i = 0; i < sockets.size(); i++){
    FD_SET(sockets[i], &sockfds);
  }
  while(true){
    fd_set sockfds_temp = sockfds; // copy it
    if (select(fdmax+1, &sockfds_temp, NULL, NULL, NULL) == -1) {
      std::perror("select");
      return -1;
    }
    // run through the existing connections looking for data to read
    for(int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &sockfds_temp)) {
        if (i == sockets[0]) { //ringmaster
          struct potato p;
          int gameStatus = getMsg(p,i);
          if(gameStatus < 0){
            std::cerr << "get potato from host error" << std::endl;
            return -1;
          }
          else if(gameStatus > 0){
            sendEnd(sockets,1);
            sendEnd(sockets,2);
            return 0;
          }
          int status = handlePotato(p,sockets,ownNum,playerNum);
          if(status < 0){
            std::cerr << "handle potato from host error" << std::endl;
            return -1;
          }
        }
        else{ //clients
          struct potato p;
          int gameStatusClient = getMsg(p,i);
          if(gameStatusClient < 0){
            std::cerr << "get potato from client error" << std::endl;
            return -1;
          }
          else if(gameStatusClient > 0){
            sendEnd(sockets,1);
            sendEnd(sockets,2);
            return 0;
          }
          int status = handlePotato(p,sockets,ownNum,playerNum);
          if(status < 0){
            std::cerr << "handle potato from client error" << std::endl;
            return -1;
          }
        }
      }
    }
  }
  return 0;
}

/**********************************************************/
int main(int argc, char *argv[]){
  
  if (argc != 3){
    std::cerr<<"wrong num of parameter" << std::endl;
    return EXIT_FAILURE;
  }
  //connect to host 
  char* hostName = argv[1];
  char* PORT = argv[2];
  int sockfd_to_host = 0;
  int sockfd_own = 0;
  int sockfd_to_other = 0;
  int sockfd_from_other = 0;
  
  connectToSocket(hostName,PORT, sockfd_to_host);
  
  //setup own socket, send port num to server
  const char* PORT_LISEN = "0";
  setupSocket(PORT_LISEN, sockfd_to_host, sockfd_own);
  
  //recieve port and host
  int  numbytes;
  std::string recv;
  if(!recieve(sockfd_to_host,&numbytes,recv)){
    std::cerr << "get hostname: port number" << std::endl;
  }

  //std::cout << recv << std::endl;
 
  std::vector<std::string> hostPort;
  hostPort = parse_host_port(recv);
  //std::cout << hostPort[0].c_str() <<hostPort[1].c_str() << " : " << hostPort[2].c_str() << std::endl;
  
  //connect with each other 
  if((connectToSocket(hostPort[1].c_str(),hostPort[2].c_str(),sockfd_to_other)) < 0){
    std::cerr << "connect other client error" << std::endl;
    return EXIT_FAILURE;
  }
  
  //accept another client
  if((sockfd_from_other = acceptSocket(sockfd_own)) < 0){
    std::cerr << "accept other client error" << std::endl;
    return EXIT_FAILURE;
  }

  //start to select and play the game
  std::vector<int> sockets({sockfd_to_host,sockfd_to_other,sockfd_from_other});
  if(selectPort(sockets, std::stoi(hostPort[0]), std::stoi(hostPort[3])) < 0){
    std::cerr << "select client error" << std::endl;
    return EXIT_FAILURE;
  }
  
  close(sockfd_to_host);
  close(sockfd_own);
  close(sockfd_to_other);
  close(sockfd_from_other);
  
  return EXIT_SUCCESS;
}
