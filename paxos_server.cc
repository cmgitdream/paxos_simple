
/*
 * paxos server
 * */

#include "paxos_common.h"
#include "paxos_message.h"

int main(int argc, char *argv[])
{
  char *ip = "10.0.11.212"; 
  uint16_t port = 10010;
  int epoll_fd = -1;
  int listen_fd = -1;
  struct sockaddr_in mysock;
  struct epoll_event events[EPOLL_MAX_EVENTS];
 
  if (socket_init(ip, port, &listen_fd, &mysock) < 0)
  {
    printf("socket_init failed\n");
    return -1;
  }

  set_role(PAXOS_ACCEPTER);
 
  epoll_init(&epoll_fd, listen_fd); 
  // maybe use a thread for epoll_run
  epoll_run(epoll_fd, listen_fd, events, EPOLL_MAX_EVENTS);

  return 0;
}
