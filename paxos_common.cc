
#include <iostream>
#include <assert.h>
#include "paxos_common.h"


int create_dir(char *path)
{
  int ret = mkdir(path, DIR_MODE);
  if (ret < 0 && errno != EEXIST) 
    return ret;
  return 0;
}

int write_data(int fd, char *buf, off_t off, uint64_t len)
{
  int r;
  if (off > 0) {
    r = lseek(fd, off, SEEK_CUR);
    if (r < 0)
      return -1;
  }
  r = write(fd, buf, len);
  if (r < 0)
    return -1;
  return r;
}

int write_data2(char *path, char *buf, off_t off, uint64_t len)
{
  int fd, r;
  fd = open(path, O_CREAT|O_RDWR, FILE_MODE);
  if (fd < 0)
    return -1;
  r = write_data(fd, buf, off, len);
  close(fd);
  return r;
}

int write_num(char *path, uint64_t num)
{
  int fd, r;
  fd = open(path, O_CREAT|O_RDWR, FILE_MODE);
  if (fd < 0)
    return -1;

  char *buf = (char *)&num;
  int len = sizeof(uint64_t)/sizeof(char);
  
  r = write_data(fd, buf, 0, len);
  if (r < 0)
    return r;
  close(fd);
  return r;
}


int read_data(int fd, char *buf, off_t off, uint64_t len)
{
  int r;
  if (off > 0) {
    r = lseek(fd, off, SEEK_CUR);
    if (r < 0)
      return -1;
  }
  r = read(fd, buf, len);
  if (r < 0)
    return -1;
  return r;
}

int read_data2(char *path, char *buf, off_t off, uint64_t len)
{
  int fd, r;
  fd = open(path, O_RDWR, FILE_MODE);
  if (fd < 0)
    return -1;
  r = read_data(fd, buf, 0, len);
  if (r < 0)
    return r;
  close(fd);
  return r;
}

int read_num(char *path, uint64_t *num)
{
  int fd, r;
  fd = open(path, O_RDWR, FILE_MODE);
  if (fd < 0)
    return -1;
  
  char *buf = (char *)num;
  int len = sizeof(uint64_t)/sizeof(char);
  r = read_data(fd, buf, 0, len);
  if (r < 0)
    return r;
  //std::cout << __func__ << ": num = " << *num << std::endl;  
  close(fd);
  return r;
}

int Proposer::send_prepare_request(int epoll_fd, int sock_fd, uint64_t send_pn) {
    Message *m = create_message(PAXOS_PREPARE_REQUEST);
    PaxosPrepareRequest *req = (PaxosPrepareRequest *)m;
    req->rank = rank;
    req->epoll_fd = epoll_fd;
    req->fd = sock_fd;
    req->pn = send_pn;
    msgr->submit_message(sock_fd, (Message *)m);
    add_event(epoll_fd, sock_fd, EPOLLOUT);
    //modify_event(epoll_fd, sock_fd, EPOLLIN|EPOLLOUT);
    //std::cout << "Proposer::" << __func__ << ": send pn = " << req->pn << std::endl;
    return 0;
  }

int Proposer::send_accept_request(int epoll_fd, int sock_fd, uint64_t accept_pn,
	std::string accept_value)
  {
    Message *m = create_message(PAXOS_ACCEPT_REQUEST);
    PaxosAcceptRequest *req = (PaxosAcceptRequest *)m;
    req->rank = rank;
    req->epoll_fd = epoll_fd;
    req->fd = sock_fd;
    req->accept_pn = accept_pn;
    req->accept_value = accept_value;
    msgr->submit_message(sock_fd, (Message *)m);
    add_event(epoll_fd, sock_fd, EPOLLOUT);
    //modify_event(epoll_fd, sock_fd, EPOLLIN|EPOLLOUT);
    //std::cout << "Proposer::" << __func__ << ": accept pn = " << req->pn << std::endl;
    return 0;
  }

int Proposer::broadcast_prepare_request(int efd, uint64_t pn)
  {
    int r = 0;
    // send prequest to quorum members
    std::set<Socket *>::iterator itr = member->quorum->get_quorum().begin();
    for (; itr != member->quorum->get_quorum().end(); ++itr) {
	
      // todo: checkout connect state
      if( msgr->netpeer->connect_peer2(*itr) < 0) {
	std::cout << __func__ << ": connect_peer2 failed" << std::endl;
	return -1;
      }
      //std::cout << __func__ << ": connect fd = " << msgr->netpeer->np_peerfd
//	<< std::endl;
      r = send_prepare_request(efd, msgr->netpeer->np_peerfd, pn);
      if (r < 0)
        return -1;
    }
    std::cout << __func__ << ": proposer rank = " << rank
	<< ", will write persist data" << std::endl;
    write_persist_data();
    return 0;
  }

int Proposer::broadcast_accept_request(int efd, uint64_t pn, std::string accept_value)
  {
    int r = 0;
    // send prequest to prepare_agree_set members
    std::map<int, int>::iterator itr = prepare_agree_map.begin();
    for (; itr != prepare_agree_map.end(); ++itr) {
	
      // we assume connected to netpeer
      int fd = itr->second;
      //std::cout << __func__ << ": peer fd = " << fd
//	<< std::endl;
      r = send_accept_request(efd, fd, pn, accept_value);
      if (r < 0)
        return -1;
    }
    return 0;
  }

int Proposer::process_prepare_response(PaxosPrepareResponse *m) {
    //std::cout << "PaxosPrepareResponse = " << *m << std::endl;
    // sent_pn has be updated by gen_pn2() in Paxos::process_command()
    assert(sent_pn != 0);
    if (m->reply_pn != sent_pn) {
      std::cout << "Proposer::" << __func__
	<< ": this is not my excepted prepare_response" << std::endl;
    }
    // responsed by one accepter
    int quorum_count = member->quorum->get_count();
    bool do_send_accept = false;
    bool do_resend_prepare = false;

    prepare_replied++;
    if (!m->refuse) {
      passed++;
      prepare_agree_map.insert(std::make_pair(m->fd, m->fd));
    }

    // replied by all quorum members(accepters)
    if (prepare_replied == quorum_count) {
      if (passed > (quorum_count / 2)) {
	do_send_accept = true;
      } else if (retries) {
	do_resend_prepare = true;
	--retries;
      }
    }
    // i am refused by at least half accepter, choose a new pn
    if (do_resend_prepare) {
      sent_pn = gen_pn(m->responsed_pn); // choose an larger pn
//      std::cout << __func__ << " new sent_pn = " << sent_pn << std::endl;
      if (retries == 0) {
        std::cout << "Proposer::" << __func__ << ": sent_pn = " << sent_pn
		<<" retries = 0, i(rank:" << rank << ") give up this round" << std::endl;
	return 0;
      }
      prepare_replied = 0;
      passed = 0;
      prepare_agree_map.clear();
      //return send_prepare_request(m->epoll_fd, m->fd, sent_pn);
      return broadcast_prepare_request(m->epoll_fd, sent_pn);
    }
    
    // i am passed by accepter in prepare phase
    if (member_max_pn < m->accepted_pn) {
      member_max_pn = m->accepted_pn;
      member_max_value = m->value;
    }
    std::cout << "Proposer::" << __func__ << ": pn = " << sent_pn
	<<" i(rank:" << rank << ") am passed by " << passed
	<< " accepters" << std::endl;
    // if passed by majority
    if (do_send_accept) {
      if (member_max_value.length() == 0) {
	std::stringstream ss;
	ss << "pn: " << sent_pn << " win this round";
	member_max_value = ss.str();
      }
      //send_accept_request(m->epoll_fd, m->fd, sent_pn, member_max_value);
      broadcast_accept_request(m->epoll_fd, sent_pn, member_max_value);
    }

    if (prepare_replied >= quorum_count) {
      prepare_replied = 0;
      accept_requires = passed;
      passed = 0;
      prepare_agree_map.clear();
    }
    return 0; 
  }

int Proposer::process_accept_response(PaxosAcceptResponse *m) {
    //std::cout << "PaxosAcceptResponse = " << *m << std::endl;
    if (m->reply_pn != sent_pn) {
      std::cout << "Proposer::" << __func__
	<< ": this is not my excepted accept_response" << std::endl;
    }
    int quorum_count = member->quorum->get_count();
    bool success = false;
    accept_replied++;
    if (!m->refuse) {
      accepted++;
      accept_agree_map.insert(std::make_pair(m->fd, m->fd));
    }
    if (accept_replied >= accept_requires) {
      if (accepted > (quorum_count / 2)) {
	std::cout << "Proposer::" << __func__ << ": pn = " << sent_pn
		<< ", i(rank:" << rank << ") am accepted by majority, success" << std::endl;
      } else {
	std::cout << "Proposer::" << __func__ << ": pn = " << sent_pn
		<< ", i(rank:" << rank << ") am NOT accepted by majority, FAILED" << std::endl;
      }
      // clear accepted
      accept_replied = 0;
      accepted = 0;
      accept_agree_map.clear();
    }
    return 0;
  }
