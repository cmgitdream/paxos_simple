
#include <pthread.h>
#include <iostream>
#include "paxos_common.h"

int main(int argc, char *argv[])
{
/*
  char *ip = "10.0.11.212"; 
  uint16_t port = 10010;
  int epoll_fd = -1;
  int sock_fd = -1;
  struct sockaddr_in server_sock;
  struct epoll_event events[EPOLL_MAX_EVENTS];
 
  if (client_socket_init(ip, port, &sock_fd, &server_sock) < 0)
  {
    printf("socket_init failed\n");
    return -1;
  }
 
  client_epoll_init(&epoll_fd, sock_fd); 
  set_role(PAXOS_PROPOSER);

  struct event_callback ecb;
  ecb.rcb = handle_read_fd;
  ecb.wcb = handle_write_fd;
  // maybe use a thread for epoll_run
  epoll_work(epoll_fd, sock_fd, events, EPOLL_MAX_EVENTS, &ecb);
*/
  Commander command;
  if (command.parse_command(argc, argv) < 0)
    return -1;
  command.send_paxos_command(argv[3], argv[4]);
  return 0;
}


