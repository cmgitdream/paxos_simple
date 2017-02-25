
#ifndef PAXOS_MESSENGER_H
#define PAXOS_MESSENGER_H

#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>

#include <map>
#include <queue>
#include <list>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "paxos_message.h"

#define EPOLL_FDSIZE 1024
#define EPOLL_MAX_EVENTS 32
#define MSG_SIZE 255

class Messenger;
class Commander;

typedef int (*accept_callback)(int, int);
typedef int (*read_callback)(int, int, int);
typedef int (*write_callback)(int, int, int);

typedef int (*handle_cb)(int, int, int);

struct event_callback {
  accept_callback accept_cb;
  read_callback read_cb;
  write_callback write_cb;  
};

int set_nonblock(int fd);

int do_socket_init(char *ip, uint16_t port, int *initfd, struct sockaddr_in *sock);
int socket_init(char *ip, uint16_t port, int *listen_fd, struct sockaddr_in *sock);
int client_socket_init(char *ip, uint16_t port, int *fd, struct sockaddr_in *sock);
int epoll_init(int *epoll_fd, int listen_fd);
int client_epoll_init(int *epoll_fd, int listen_fd);

int sync_recv_message(int fd, Message **mptr);
int sync_send_message(int fd, Message *m);
void add_event(int epoll_fd, int fd, int flag);
void rm_event(int epoll_fd, int fd, int flag);
void modify_event(int epoll_fd, int fd, int flag);

Message *create_message(int type);

void print_sock(struct sockaddr_in *sock);

struct epoll_arg {
  int epoll_fd;
  int listen_fd;
  struct epoll_event *events;   
  int ev_count;
};

class Socket {
public:
  struct sockaddr_in sock;
  Socket()
  { 
    memset(&sock, 0, sizeof(sock));
  }
  int init(char *ip, uint16_t port)
  {
    int r = -1;
    struct in_addr ipaddr;
    if (inet_pton(AF_INET, ip, &ipaddr) < 0)
      return -1;
    sock.sin_family = AF_INET;
    sock.sin_addr.s_addr = ipaddr.s_addr;
    sock.sin_port = htons(port);
    return 0;
  }
  int newfd() 
  {
    int nfd = socket(AF_INET, SOCK_STREAM, 0);
    if (nfd < 0) {
      printf("create socket failed, errono = %d\n", errno);
      return -1;
    }
    std::cout << __func__ << ": create fd = " << nfd << std::endl;

/*
    int on = 1;
    int opt_len = sizeof(int);
    setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)opt_len);
*/
    return nfd;
  }
};

class Epoll {
public:
  int epoll_fd;
  uint32_t ev_max;
  struct epoll_event *events; 
  Epoll(uint32_t max = EPOLL_MAX_EVENTS) : ev_max(max)
  {
    events = (struct epoll_event *)
	malloc(ev_max * sizeof(struct epoll_event)); 
    assert(events != NULL);
  }
  ~Epoll()
  {
    if (events)
      free(events);
  }
  int init()
  {
    epoll_fd = epoll_create(EPOLL_FDSIZE);
    if (epoll_fd < 0)
      return -1;
    return 0;
  }
  int epoll_work(int epoll_fd, int listen_fd, struct epoll_event *events,
	int count, struct event_callback *ecb);
};

class NetPeer {
public:
  Socket np_sock;
  int np_myfd;
  int np_peerfd;
  Epoll np_epoll;
  struct event_callback np_ecb;
  NetPeer() : np_myfd(-1),np_peerfd(-1)
  {
    memset(&np_ecb, 0, sizeof(struct event_callback));
  }
  int bind_listen()
  {
    int r = 0;
    r = bind(np_myfd, (struct sockaddr *)&np_sock.sock, sizeof(struct sockaddr));
    if (r < 0) {
      printf("bind failed, errno = %d\n", errno);
      return -1;
    }

    set_nonblock(np_myfd);
    r = listen(np_myfd, 5);
    return r;
  }
  int init(char *ip, uint16_t port)
  {
    int r = 0;
    if (np_epoll.init() < 0)
      return -1;
    r = np_sock.init(ip, port);
    if (r < 0)
      return r;
    np_myfd = np_sock.newfd();
    if (np_myfd < 0)
      return -1;
    r = bind_listen();
    if (r < 0)
      return -1;
    std::cout << "NetPeer::init success" << std::endl;
  }
  int connect_peer(char *ip, uint16_t port)
  {
    Socket peer_sock;
    peer_sock.init(ip, port); 
    np_peerfd = peer_sock.newfd();  
    return connect(np_peerfd, (struct sockaddr *)&peer_sock.sock,
	(socklen_t)sizeof(struct sockaddr));
  }
  int connect_peer2(Socket *addr)
  {
    np_peerfd = addr->newfd();
    return connect(np_peerfd, (struct sockaddr *)&addr->sock,
	(socklen_t)sizeof(struct sockaddr));
  }
  int set_event_callback(accept_callback _acb, read_callback _rcb, write_callback _wcb)
  {
    np_ecb.accept_cb = _acb;
    np_ecb.read_cb = _rcb;
    np_ecb.write_cb = _wcb;
  }
  int run()
  {
    add_event(np_epoll.epoll_fd, np_myfd, EPOLLIN);
    std::cout << "NetPeer::run epoll_fd = " 
	<< np_epoll.epoll_fd
	<<  ", listen_fd = " << np_myfd 
	<< ", max_events = " << np_epoll.ev_max << std::endl;
    np_epoll.epoll_work(np_epoll.epoll_fd, np_myfd, np_epoll.events,
	np_epoll.ev_max, &np_ecb);
    return 0;
  }
};

class MessengerUser {
public:
  Messenger *msgr;
  int user_id;
  MessengerUser(Messenger *_msgr, int id) : msgr(_msgr), user_id(id)
  {}
  virtual int dispatch_message(Message *m) = 0;
};

class Messenger {
public:
  NetPeer *netpeer;
  static std::map<int, std::queue<Message *> *> send_map;
  static std::map<int, std::queue<Message *> *> recv_map;
  static std::map<int, Message *> pending_read;

  static std::map<int, MessengerUser *> m_users;

  Messenger() { netpeer = new NetPeer();}
  ~Messenger() { delete netpeer; }

  static Message *get_pending(int fd);
  static void set_pending(int fd, Message *m);
  static int clear_pending(int fd);
  
  static int set_send_queue(int fd, std::queue<Message *>* qtr);
  static std::queue<Message *>* get_send_queue(int fd);
  static void clear_queue(std::queue<Message *> *qtr);
  static void clear_send_queue(int fd);
  
  static void clear_send_map();

  static int set_recv_queue(int fd, std::queue<Message *>* qtr);
  static std::queue<Message *>* get_recv_queue(int fd);
  static void clear_recv_queue(int fd);
  static void clear_recv_map();


  static ssize_t recv_msg_part(int epoll_fd, int fd, void *buf, size_t len);
  static ssize_t recv_as_many(int epoll_fd, int fd, void *buf, size_t len);
  static ssize_t recv_msg_remind(int epoll_fd, int fd, void *buf, size_t len,
	uint64_t *total);
  static bool atomic_recv(int epoll_fd, int fd, void *buf, size_t len);

  int submit_message(int fd, Message *m);
  static int send_one_message(int epoll_fd, int fd);

  static int accept_peer(int epoll_fd, int listen_fd);
  static int handle_read_message(int epoll_fd, int fd);
  static int dispatch_message(Message *m);

  static int process_accept(int epoll_fd, int listen_fd)
  {
    return accept_peer(epoll_fd, listen_fd);
  }
  static int process_net_in(int epoll_fd, int fd, int sock_fd)
  {
    return handle_read_message(epoll_fd, fd);
  }
  static int process_net_out(int epoll_fd, int fd, int sock_fd)
  {
    return send_one_message(epoll_fd, fd);
  }

  int add_messenger_user(MessengerUser *m_user)
  {
    m_users[m_user->user_id] = m_user;
    return 0;
  }

  int remove_messenger_user(int id)
  {
    m_users.erase(id);
    return 0;
  }

  int init(char *ip, uint16_t port)
  {
    netpeer->init(ip, port);
    netpeer->set_event_callback(&process_accept, &process_net_in, &process_net_out);
    return 0;
  }

  int run()
  {
    netpeer->run();
    return 0;
  }
  
};

class Commander {
public:
  Socket *server_sock;
  int conn_fd;
  Commander()
  {
    server_sock = new Socket();
  }
  ~Commander()
  {
    delete server_sock;
  }
  void usage() 
  {
    std::stringstream ss;
    ss << "usage:\n"
	<< "\t<ip> <port> prepare <pn>\n";
    std::cout << ss.str() << std::endl;
  }
  int set_address(char *ip, uint16_t port)
  {
    server_sock->init(ip, port);
    conn_fd = server_sock->newfd();
    return 0;
  }
  int parse_command(int argc, char *argv[])
  {
    if (argc < 5) {
      usage();
      return -1;
    }
    if (strcmp(argv[3], "prepare") != 0) {
      usage();
      return -1;
    }
    set_address(argv[1], strtoul(argv[2], NULL, 10));  
    return 0;
  }
  int send_paxos_command(char *name, char *val)
  {
    int r = 0;
    r = connect(conn_fd, (struct sockaddr *)&server_sock->sock,
	(socklen_t)sizeof(struct sockaddr)); 
    if (r < 0) {
      std::cout << __func__ << ": connect server failed" << std::endl;
      return r;
    }
    PaxosCommandRequest* req = (PaxosCommandRequest *)create_message(PAXOS_COMMAND_REQUEST);
    req->name = name;
    req->val = val;
    PaxosCommandResponse* reply = NULL;
    Message *m;

    req->encode_payload();
      r = sync_send_message(conn_fd, req); 
    /*
    while (1)
    {
      r = sync_send_message(conn_fd, req); 
      if (r < 0)
	continue;
      break;
    }
    while (1)
    {
    */
      r = sync_recv_message(conn_fd, &m);
      //if (r < 0)
//	continue;
      reply = (PaxosCommandResponse *)m;
      reply->decode_payload();
      std::cout << __func__ << " comand result = " << reply->result << std::endl;
  //    break; 
    //}
    if (req)
      delete req;
    if (reply)
      delete reply;
    close(conn_fd);
    return 0;
  }
};

#endif

