
#include <iostream>
#include <sstream>
#include "paxos_messenger.h"

#define EPOLL_FDSIZE 1024
#define EPOLL_MAX_EVENTS 32

#if 0
// <fd, queue>
std::map<int, std::queue<Message *> *> send_map;
std::map<int, std::queue<Message *> *> recv_map;
std::map<int, Message *> pending_read;
#endif

//Messenger::
Message *Messenger::get_pending(int fd)
{
  std::map<int, Message *>::iterator itr = pending_read.find(fd);
  if (itr != pending_read.end())
    return itr->second;
  return NULL;
}

void Messenger::set_pending(int fd, Message *m)
{
  assert(get_pending(fd) == NULL);
  pending_read[fd] = m;  
}

int Messenger::clear_pending(int fd)
{
  pending_read.erase(fd);
}

int Messenger::set_send_queue(int fd, std::queue<Message *>* qtr)
{
  send_map[fd] = qtr;
  return 0;
}

std::queue<Message *>* Messenger::get_send_queue(int fd)
{
  std::map<int ,std::queue<Message *>*>::iterator itr;
  itr = send_map.find(fd);
  if (itr != send_map.end())
    return itr->second;
  return NULL;
}

void Messenger::clear_queue(std::queue<Message *> *qtr)
{
  if (qtr) {
    while(!qtr->empty()) {
      delete qtr->front();
      qtr->pop();
    }
  }
}

void Messenger::clear_send_queue(int fd)
{
  std::queue<Message *> *qtr = get_send_queue(fd);
  // todo: process the message is sent to peer ?
  clear_queue(qtr);
}

void Messenger::clear_send_map()
{
  std::map<int ,std::queue<Message *>*>::iterator itr;
  itr = send_map.begin();
  for (; itr != send_map.end(); itr++) {
    clear_queue(itr->second);
    delete itr->second;
  }
  send_map.clear();
}

int Messenger::set_recv_queue(int fd, std::queue<Message *>* qtr)
{
  recv_map[fd] = qtr;
  return 0;
}

std::queue<Message *>* Messenger::get_recv_queue(int fd)
{
  std::map<int ,std::queue<Message *>*>::iterator itr;
  itr = recv_map.find(fd);
  if (itr != recv_map.end())
    return itr->second;
  return NULL;
}

void Messenger::clear_recv_queue(int fd)
{
  std::queue<Message *> *qtr = get_recv_queue(fd);
  clear_queue(qtr);
}

void Messenger::clear_recv_map()
{
  std::map<int ,std::queue<Message *>*>::iterator itr;
  itr = recv_map.begin();
  for (; itr != recv_map.end(); itr++) {
    clear_queue(itr->second);
    delete itr->second;
  }
  recv_map.clear();
}

int set_nonblock(int fd)
{
  int flags;
  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;

  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
  return 0;
}

int do_socket_init(char *ip, uint16_t port, int *initfd, struct sockaddr_in *sock)
{
  int r = -1;
  struct in_addr ipaddr;
  if (inet_pton(AF_INET, ip, &ipaddr) < 0)
    return -1;
  sock->sin_family = AF_INET;
  sock->sin_addr.s_addr = ipaddr.s_addr;
  sock->sin_port = htons(port);

  *initfd = socket(AF_INET, SOCK_STREAM, 0);
  if (*initfd < 0) {
    printf("create socket failed, errono = %d\n", errno);
    return -1; 
  }

  int on = 1;
  int opt_len = sizeof(int);
  setsockopt(*initfd, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)opt_len);
 
  return 0;
}

int socket_init(char *ip, uint16_t port, int *listen_fd, struct sockaddr_in *sock)
{
  int r = 0;
  r = do_socket_init(ip, port, listen_fd, sock);
  if (r < 0)
    return -1;
  r = bind(*listen_fd, (struct sockaddr *)sock, sizeof(struct sockaddr));
  if (r < 0) {
    printf("bind failed, errno = %d\n", errno);
    return -1;
  }

  set_nonblock(*listen_fd);
  listen(*listen_fd, 5);  
  return 0;
}

int client_socket_init(char *ip, uint16_t port, int *fd, struct sockaddr_in *sock)
{
  int r = 0;
  r = do_socket_init(ip, port, fd, sock);
  connect(*fd, (struct sockaddr *)sock, (socklen_t )sizeof(struct sockaddr));
  return r;
}

int epoll_init(int *epoll_fd, int fd)
{
  int efd = -1;
  efd = epoll_create(EPOLL_FDSIZE);  
  if (efd < 0) {
    printf("epoll_create failed, errno = %d\n", errno);
    return -1;
  }
  *epoll_fd = efd;
  struct epoll_event init_event; // listen connection from peer
  init_event.data.fd = fd;
  init_event.events = EPOLLIN;
  
  if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &init_event)) {
    printf("add listen event to epoll failed\n");
    return -1;
  }
  // now epoll = [fd, ...]
  return 0;
}

int client_epoll_init(int *epoll_fd, int fd)
{
  int efd = -1;
  efd = epoll_create(EPOLL_FDSIZE);  
  if (efd < 0) {
    printf("epoll_create failed, errno = %d\n", errno);
    return -1;
  }
  *epoll_fd = efd;
  // EPOLLOUT: when tcp socket writbuf is not full, trigger EPOLLOUT event
  // EPOLLIN: when tcp socket recv message, trigger EPOLLIN event
  add_event(efd, STDIN_FILENO, EPOLLIN);
  add_event(efd, fd, EPOLLIN|EPOLLOUT);
  //add_event(efd, STDOUT_FILENO, EPOLLOUT);
  //add_event(efd, STDOUT_FILENO, EPOLLOUT);
  // now epoll = [fd, ...]
  return 0;
}

void add_event(int epoll_fd, int fd, int flag)
{
  struct epoll_event ev;
  ev.events = flag;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void rm_event(int epoll_fd, int fd, int flag)
{
  struct epoll_event ev;
  ev.events = flag;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}

void modify_event(int epoll_fd, int fd, int flag)
{
  struct epoll_event ev;
  ev.events = flag;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void print_sock(struct sockaddr_in *sock)
{
  char ipstr[32];  
  memset(ipstr, '\0', 32);
  inet_ntop(AF_INET, &sock->sin_addr, ipstr, 32);
  printf("%s:%d\n", ipstr, ntohs(sock->sin_port));
}

int Messenger::accept_peer(int epoll_fd, int listen_fd)
{
  struct sockaddr_in peer_sock;
  int len = sizeof(struct sockaddr_in);
  int peer_fd = accept(listen_fd, (struct sockaddr *)&peer_sock,
	(socklen_t *)&len);
  if (peer_fd < 0) {
    printf("accept peer failed\n");
    return -1;
  }
  printf("accept new fd = %d, ", peer_fd);
  print_sock(&peer_sock);
  set_nonblock(peer_fd);
 
  // save peer_sock & add peer_fd to epoll ?
  //peer_map[peer_sock] = peer_fd;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = peer_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, peer_fd, &ev);
  
  // now epoll_fdset = [listen_fd, peer_fd1, ...]
  return 0;
}

int Messenger::submit_message(int fd, Message *m)
{
  std::queue<Message *> *qtr = get_send_queue(fd);
  if (!qtr) {
   qtr = new std::queue<Message *>(); 
   set_send_queue(fd, qtr);
  }
  m->encode_payload();
  //std::cout << __func__ << ": message type = " << message_type(m->header.type)
  //	<< ", encoded str = " << m->dss.str() << std::endl;
  qtr->push(m);
  modify_event(m->epoll_fd, m->fd, EPOLLIN|EPOLLOUT);
  return 0;
}

int recv_entire(int fd, char *buf, int len)
{
  int pos = 0;
  int xlen = 0;
  while (pos < len) {
    xlen = recv(fd, buf + pos, len - pos, 0);
    if (xlen < 0)
      return -1;
    pos += xlen;
  }
  return pos;
}

int send_entire(int fd, const char *buf, int len)
{
  int pos = 0;
  int xlen = 0;
  while (pos < len) {
    xlen = send(fd, buf + pos, len - pos, 0);
    if (xlen < 0)
      return -1;
    pos += xlen;
  }
  return pos;
}

int sync_send_message(int fd, Message *m)
{
  if (!m)
    return -1;
  int xlen = 0;
  int sent = 0;
  if (!m->header_wrote) {
    if ((xlen = send_entire(fd, (char *)&(m->header), sizeof(m->header))) < 0) {
      std::cout << "msg header send failed, will resend" << std::endl;
      return -1;
    }
    //std::cout << __func__ << ": msg header sent successfull" << std::endl;
    m->header_wrote = true;
    sent += xlen;
  }
  int msg_size = m->dss.str().size();
  //std::cout << __func__ <<":msg_size = " << msg_size << std::endl;
  int len = 0;

  // send payload
  const char *ptr = m->dss.str().c_str();
  xlen = send_entire(fd, ptr, msg_size);
  if (xlen < 0)
    return -1;
  m->sent += xlen;
  sent += xlen;
  // message is sent completely
  if (m->sent == msg_size) {
    //std::cout << __func__ << ":send finish msg type = " << message_type(m->header.type) << std::endl;
    //std::cout << __func__ << ":sent size = " << m->sent << std::endl;
    //delete m;
    sent += m->sent;
  }
  //if (sent != (sizeof(m->header) + msg_size))
  //  return -1;
  return sent;
}

int sync_recv_message(int fd, Message **mptr)
{
  struct msg_header header; 
  char* ptr =(char *)&header; 
  int to_read = sizeof(header);
  int xlen = 0;
  int nread = 0;
  Message *m;

  *mptr = NULL;
  if ((xlen = recv_entire(fd, ptr, to_read)) < 0) {
    std::cout << __func__ << ":atomic recv header failed, fd = "
	<< fd << std::endl;
      return -1;
  }
  nread += xlen;

  //std::cout << __func__ << ":recv header.type = "
//	<< message_type(header.type) << std::endl;
  //std::cout << __func__ << ":payload_len = "
//	<< header.payload_len << std::endl;
  m = create_message(header.type);
  m->fd = fd;
  memcpy(&m->header, &header, sizeof(header));

  // now we can read msg payload
  // todo: control payload_len
  to_read = m->header.payload_len;
  m->payload_buf = new char[to_read];
  memset(m->payload_buf, '\0', to_read);
  ptr = m->payload_buf;
  xlen = recv_entire(fd, ptr, to_read);
  if (xlen < 0)
    return -1;
  nread += xlen;
  *mptr = m;
  return nread;
}


int send_message_part(int epoll_fd, int fd, const char *buf, int len)
{
  int n = 0;
  struct epoll_event ev;
  n = send(fd, buf, len, 0);
  if (n < 0)
    std::cout << __func__ << ": errno = " << errno
	<< "(" << strerror(errno) << std::endl;
  if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    std::cout << __func__ << ": send again ..." << std::endl;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    return -1;
  }
  if (n != len) {
    std::cout << __func__ << ": NOT send full n=" << n
	<< " != len=" << len << std::endl;
  } else {
    //std::cout << __func__ << ": send buf full n=" << n << std::endl;
  }
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
  return n;
}

bool atomic_send(int epoll_fd, int fd, const char *buf, size_t len)
{
  ssize_t nread;
  // send all or nothing
  nread = send_message_part(epoll_fd, fd, buf, len);
  if (nread < 0 || nread != len) {
    return false;
  }
  return true;
}

int Messenger::send_one_message(int epoll_fd, int fd)
{
  std::queue<Message *> *qtr = get_send_queue(fd);
  if (!qtr)
    return 0;
  Message *m = qtr->front();
  if (!m)
    return -1;

  //std::cout << __func__ <<":send msg type = " << message_type(m->header.type) << std::endl;
  int n = 0;
  const char *ptr = NULL;
  int pos = m->sent;
  // atomic(resend) send msg_header
  if (!m->header_wrote) {
    if (!atomic_send(epoll_fd, fd, (char *)&(m->header), sizeof(m->header))) {
      std::cout << "msg header send failed, will resend" << std::endl;
      return -1;
    }
    //std::cout << __func__ << ": msg header sent successfull" << std::endl;
    m->header_wrote = true;
  }
  int msg_size = m->dss.str().size();
  //std::cout << __func__ <<": msg_size = " << msg_size << std::endl;
  int len = 0;

  // send payload
  do {
    ptr = m->dss.str().c_str() + m->sent;
    len = msg_size - m->sent;
    n = send_message_part(epoll_fd, fd, ptr, len);
    if (n < 0)
      return n;
    else if (n > 0)
      m->sent += n;
    //std::cout << __func__ <<":current sent = " << m->sent << std::endl;
  } while (m->sent < msg_size);
  // message is sent completely
  if (m->sent == msg_size) {
    qtr->pop();
    //std::cout << __func__ <<":send finish msg type = " << message_type(m->header.type) << std::endl;
    //std::cout << __func__ <<":sent size = " << m->sent << std::endl;
    delete m;
  }

  return 0;
}

Message *create_message(int type)
{
  Message *m = NULL;
  switch(type) {
  case PAXOS_PREPARE_REQUEST:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosPrepareRequest();
    break;
  }
  case PAXOS_PREPARE_RESPONSE:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosPrepareResponse();
    break;
  }
  case PAXOS_ACCEPT_REQUEST:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosAcceptRequest();
    break;
  }
  case PAXOS_ACCEPT_RESPONSE:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosAcceptResponse();
    break;
  }
  case PAXOS_COMMAND_REQUEST:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosCommandRequest();
    break;
  }
  case PAXOS_COMMAND_RESPONSE:
  {
    //std::cout << __func__ << " type = " << message_type(type) << std::endl;
    m = new PaxosCommandResponse();
    break;
  }
  default:
    printf("error message type\n");
  }
  return m;
}

ssize_t Messenger::recv_msg_part(int epoll_fd, int fd, void *buf, size_t len)
{
  bool peer_closed = false;
  ssize_t nread = 0;
  nread = recv(fd, buf, len, 0);//MSG_DONTWAIT);  
  if (nread < 0) { 
    std::cout << __func__ << ": errno = " << strerror(errno) << std::endl;
    // peer disconnected
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      peer_closed = true;
    }
    else {
      std::cout << " EAGAIN or EWOULBLOCK, wait to retry recv..." << std::endl;
      // wait more data
      return -errno;
    }
  }
  if (nread == 0) {
    std::cout << __func__ << ":recv 0 bytes, peer NOT send data ..." << std::endl;
    peer_closed = true;
  }
  if (peer_closed) {
    printf("%s: peer fd %d has been closed\n", __func__, fd);
    clear_send_queue(fd);
    rm_event(epoll_fd, fd, EPOLLIN); 
    close(fd);
    return -1;
  }
  return nread;
}

// recv nonblock
ssize_t Messenger::recv_as_many(int epoll_fd, int fd, void *buf, size_t len)
{
  ssize_t nread = 0;
  ssize_t xread = 0;
  char *ptr = (char *)buf;
  size_t to_read = len;
  do {
    xread = recv_msg_part(epoll_fd, fd, (void *)ptr, to_read);
    if (xread < 0)
      return xread;
    to_read -= xread;
    nread += xread;
    ptr += xread;
  } while (nread < len);
  return nread;
}

ssize_t Messenger::recv_msg_remind(int epoll_fd, int fd, void *buf, size_t len,
	uint64_t *total)
{
  ssize_t nread;
  nread = recv_as_many(epoll_fd, fd, buf, len);
  if (nread < 0)
    return 0;
  *total += nread;
  // recv not complete, add epoll event callback to wait
  if (nread < len) {
    modify_event(epoll_fd, fd, EPOLLIN);
    return nread;
  }
}

bool Messenger::atomic_recv(int epoll_fd, int fd, void *buf, size_t len)
{
  ssize_t nread;
  // recv all or nothing
  nread = recv_msg_part(epoll_fd, fd, buf, len);
  if (nread < 0 || nread < len) {
    modify_event(epoll_fd, fd, EPOLLIN);
    return false;
  }
  return true;
}

/*
int handle_prepare_request(PaxosPrepareRequest *m)
{
  std::cout << "PaxosPrepareRequest = " << *m << std::endl;
  // answer prepare request with prepare response
  Message *req = create_message(PAXOS_PREPARE_RESPONSE);   
  PaxosPrepareResponse *reply = (PaxosPrepareResponse *)req;
  reply->fd = m->fd;
  reply->pn = m->pn;
  std::stringstream ss;
  ss << "elect " << m->pn << " as leader";
  reply->value = ss.str();
  std::cout << __func__ << ": msg type = " << message_type(reply->header.type) << std::endl;
  submit_message(m->fd, reply);
  modify_event(m->epoll_fd, m->fd, EPOLLIN|EPOLLOUT);
  
  return 0;
}

int handle_prepare_response(PaxosPrepareResponse *m)
{
  std::cout << "PaxosPrepareResponse = " << *m << std::endl;
  return 0;
}
*/

int Messenger::dispatch_message(Message *m)
{
  /*
  switch(m->header.type) {
  case PAXOS_PREPARE_REQUEST:
  {
    PaxosPrepareRequest *req = (PaxosPrepareRequest *)m;
    //req->decode();
    return handle_prepare_request(req);
  }
  case PAXOS_PREPARE_RESPONSE:
  {
    PaxosPrepareResponse *req = (PaxosPrepareResponse *)m;
    //req->decode();
    return handle_prepare_response(req);
  }
  default:
    return -1;
  }
  */
  assert(m != NULL);
  int type_id = get_message_base_id(m->header.type);
  std::map<int, MessengerUser *>::iterator itr = m_users.find(type_id);
  if (itr == m_users.end())
    return -1;
  itr->second->dispatch_message(m);
  return 0;
}

int Messenger::handle_read_message(int epoll_fd, int fd)
{
  struct msg_header header;
  bool got_pending = false;
  uint64_t has_read = 0;
  size_t to_read = 0;
  ssize_t nread = 0;
  char *ptr = NULL;
  
  // pending message means not recv complete
  Message *m = get_pending(fd);
  if (m)
    got_pending = true;

  if (got_pending) {
    //std::cout << __func__ << ": recv pending message" << std::endl;

    // now we can read msg payload
    // todo: control payload_len
    /*
    int recvlen, len;
    len = sizeof(recvlen);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvlen, &len) < 0)
    {
      perror("getsocket error");
      return -1;
    }
    uint64_t n = to_read > recvlen ? recvlen : to_read;
    */
    to_read = m->header.payload_len - m->got;
    ptr = m->payload_buf + m->got;
    if (recv_msg_remind(epoll_fd, fd, ptr, to_read, &m->got) < to_read) {
      std::cout << __func__ << ": not recv finish, set message as pending CONTINUE" << std::endl;
      return -1;
    }
    //std::cout << __func__ << ": recv finish = " << m->got
	//<< ", clear pending"<< std::endl;
    clear_pending(fd);
  } else {
    //std::cout << __func__ << ": recv NEW message" << std::endl;
    // recv msg_header
    ptr =(char *)&header; 
    to_read = sizeof(header);
    if (!atomic_recv(epoll_fd, fd, ptr, to_read)) {
      std::cout << __func__ << ":atomic recv header failed, fd = "
	<< fd << std::endl;
      return -1;
    }

    //std::cout << __func__ << ":recv header.type = "
//	<< message_type(header.type) << std::endl;
    //std::cout << __func__ << ":payload_len = "
//	<< header.payload_len << std::endl;
    if (header.payload_len == 0) {
      std::cout << __func__ << ": payload len is 0, no need to recv payload"
	<< std::endl;
      return 0;
    }
    m = create_message(header.type);
    m->epoll_fd = epoll_fd;
    m->fd = fd;
    memcpy(&m->header, &header, sizeof(header));

    // now we can read msg payload
    // todo: control payload_len
    to_read = m->header.payload_len;
    m->payload_buf = new char[to_read];
    memset(m->payload_buf, '\0', to_read);
    ptr = m->payload_buf;
    //m->dss.rdbuf()->pubsetbuf(m->payload_buf, to_read);
    int nread = recv_msg_remind(epoll_fd, fd, ptr, to_read, &m->got);
    //std::cout << __func__ << ": recv nread=" << nread
//	<< ", to_read=" << to_read << std::endl;
    if (m->got < to_read) {
      std::cout << __func__ << ": not recv full, set message as pending" << std::endl;
      set_pending(fd, m);
      return -1;
    }
    //std::cout << __func__ << ": recv total = " << m->got << std::endl;
  }
  if (m->got < m->header.payload_len) {
      std::cout << __func__ << ": ERROR recv NOT completed got=" << m->got
	<< " < payload_len=" << m->header.payload_len << std::endl;
      return -1;
  }
  m->decode_payload();
  dispatch_message(m);  
}

/*
int epoll_run(int epoll_fd, int listen_fd, struct epoll_event *events, int count)
{
  int i, ready;
  int timeout = 300;
  while (1) {
    ready = epoll_wait(epoll_fd, events, count, -1);
    if (ready < 0)
      continue;
    for (i = 0; i < ready; i++) {
      if (events[i].events & EPOLLERR) {
	printf("epoll wait event of fd %d error EPOLLERR\n", events[i].data.fd);
	close(events[i].data.fd);
	continue;
      } else if (events[i].events & EPOLLHUP) {
	printf("epoll wait event of fd %d error EPOLLHUP\n", events[i].data.fd);
	close(events[i].data.fd);
	continue;
      } else if (events[i].data.fd == listen_fd) {
	accept_peer(epoll_fd, listen_fd);
      } else if (events[i].events & EPOLLIN) {
	std::cout << std::endl;
	//reflect_message(epoll_fd, events[i].data.fd);
	handle_read_message(epoll_fd, events[i].data.fd);
      } else if (events[i].events & EPOLLOUT) {
	send_one_message(epoll_fd, events[i].data.fd);
      } else {
      }
    }
  }
}
*/

int Epoll::epoll_work(int epoll_fd, int listen_fd, struct epoll_event *events,
	int count, struct event_callback *ecb)
{
  //std::cout << __func__ << std::endl;
  int i, ready;
  int timeout = 300;
  while (1) {
    ready = epoll_wait(epoll_fd, events, count, -1);
    //std::cout << __func__ << ": epoll wake up!!!" << std::endl;
    if (ready < 0)
      continue;
    for (i = 0; i < ready; i++) {
      if (events[i].events & EPOLLERR) {
	printf("epoll wait event of fd %d error EPOLLERR\n", events[i].data.fd);
	close(events[i].data.fd);
	continue;
      } else if (events[i].events & EPOLLHUP) {
	printf("epoll wait event of fd %d error EPOLLHUP\n", events[i].data.fd);
	close(events[i].data.fd);
	continue;
      } else if (events[i].data.fd == listen_fd) {
	if (ecb->accept_cb)
	  ecb->accept_cb(epoll_fd, listen_fd);
      } else if (events[i].events & EPOLLIN) {
	std::cout << std::endl;
	if (ecb->read_cb)
	  ecb->read_cb(epoll_fd, events[i].data.fd, listen_fd);
      } else if (events[i].events & EPOLLOUT) {
	if (ecb->write_cb)
	  ecb->write_cb(epoll_fd, events[i].data.fd, listen_fd);
      } else {
      }
    }
  }
}

  std::map<int, std::queue<Message *> *> Messenger::send_map;
  std::map<int, std::queue<Message *> *> Messenger::recv_map;
  std::map<int, Message *> Messenger::pending_read;

  std::map<int, MessengerUser *> Messenger::m_users;


/*
void *epoll_entry(void *arg)
{
  struct epoll_arg *ep = (struct epoll_arg *)arg;
  epoll_run(ep->epoll_fd, ep->listen_fd, ep->events, ep->ev_count);
  return NULL;
}
*/
