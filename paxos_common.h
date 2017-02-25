
#ifndef PAXOS_COMMON_H
#define PAXOS_COMMON_H

#include <iostream>
#include <set>
#include <map>
#include <utility>
#include "paxos_messenger.h"

#define MAX_PROPOSER_NUM 100

#define THISIP "10.0.11.212"
#define CLIENT_PORT 10010
#define PORT_0 10020
#define PORT_1 10021
#define PORT_2 10022

typedef enum paxos_type {
  PAXOS_TYPE_PROPOSER,
  PAXOS_TYPE_ACCEPTER,
  PAXOS_TYPE_LEARNER
} paxos_type;

class Quorum {
  // rank, address
  std::set<Socket *> members;
public:
  Quorum() {}
  ~Quorum() {}
  std::set<Socket *> &get_quorum() {
    return members;
  }
  int insert_quorum(Socket *addr) {
    std::pair<std::set<Socket *>::iterator,bool> ret;
    ret = members.insert(addr);
    return ret.second;
  }
  int remove_quorum(Socket *addr) {
     return members.erase(addr); 
  }
  void clear_quorum() {
    members.clear();
  }
  int get_count() { return members.size(); }
};

class PaxosMember;

class PaxosRole {
public:
  paxos_type rtype;
  int rank;
  PaxosMember *member;
  PaxosRole(paxos_type t, int r = -1) : rtype(t), rank(r), member(NULL) {}
  virtual ~PaxosRole() {};
  void set_member(PaxosMember *mbr) { member = mbr; }
  void set_rank(int r) { rank = r; }
};

class Proposer : public PaxosRole {
public:
  uint64_t sent_pn; // highest sent pn, and it can be persisted to disk
  uint64_t instance_id; // which paxos round, persist to disk
  int retries;	// time of broadcast prequest_request
  int prepare_replied;	// count of accepter who answers prepare_request
  int accept_replied;	// count of accepter who answers accept_request
  int passed;	// count of accpeter who passes prepare_request
  int accept_requires; // count of accepter to send accept_request, equal to passed
  int accepted; // count of accepter who accepts accept_request
  uint64_t member_max_pn; // received max accepted pn of majority
  std::string member_max_value; // value of member_max_pn
  int qcount; // the count of quorum members
  Messenger *msgr;
  std::map<int, int> prepare_agree_map; // <address, fd>
  std::map<int, int> accept_agree_map;
  bool showed;
  Proposer(Messenger *_msgr, int r = -1)
    : PaxosRole(PAXOS_TYPE_PROPOSER, r), sent_pn(0), instance_id(0),
      retries(2), prepare_replied(0), accept_replied(0), passed(0),
      accept_requires(0), accepted(0), member_max_pn(0),
      qcount(1), msgr(_msgr),showed(false)
  {}
  ~Proposer() {}
  int load_persist_data()
  {
    // load data from disk, eg. file, kv
    sent_pn = MAX_PROPOSER_NUM;
    instance_id = 1;
  }

  uint64_t gen_pn(uint64_t response_pn) {
    //std::cout << __func__ << ": rank = " << rank << std::endl;
    assert(rank != -1);
    uint64_t mypn;
    // pn as 100, 101, 102, ... 200, 201, 202, ...
    mypn = (response_pn /(uint64_t)MAX_PROPOSER_NUM)*(uint64_t)MAX_PROPOSER_NUM
	+ (uint64_t)rank;
    if (mypn <= response_pn)
      mypn += MAX_PROPOSER_NUM;
    return mypn;
  }

  uint64_t gen_pn2() {
    sent_pn = gen_pn(sent_pn);
    return sent_pn;
  }

  int send_prepare_request(int epoll_fd, int sock_fd, uint64_t send_pn);
  int send_accept_request(int epoll_fd, int sock_fd, uint64_t accept_pn,
	std::string accept_value);
  int broadcast_prepare_request(int efd, uint64_t pn);
  int broadcast_accept_request(int efd, uint64_t pn, std::string accept_value);
  int process_prepare_response(PaxosPrepareResponse *m);
  int process_accept_response(PaxosAcceptResponse *m);
};

class Accepter : public PaxosRole
{
public:
  uint64_t responsed_pn; // highest responsed pn, for it will recv many prepare request from different proposers
  uint64_t accepted_pn; // highest accepted pn, persist to disk
  uint64_t instance_id; // which paxos round, persist to disk
  std::string accepted_value;
  Messenger *msgr;
  Accepter(Messenger *_msgr, int r = -1)
    : PaxosRole(PAXOS_TYPE_ACCEPTER, r),responsed_pn(0), accepted_pn(0),
      instance_id(0), msgr(_msgr)
  {}
  ~Accepter() {}
 
  // response to proposer with highest accepted pn if trust current proposer
  int process_prepare_request(PaxosPrepareRequest *m) {
    //std::cout << "PaxosPrepareRequest = " << *m << std::endl;
    // answer prepare request with prepare response
    Message *req = create_message(PAXOS_PREPARE_RESPONSE);   
    PaxosPrepareResponse *reply = (PaxosPrepareResponse *)req;
    reply->rank = rank;
    reply->epoll_fd = m->epoll_fd;
    reply->fd = m->fd;
    std::stringstream ss; 
    // current prepare pn is valid
    if (m->pn > responsed_pn) {
      responsed_pn = m->pn;
      reply->refuse = 0; // valid
      std::cout << "Accepter::" << __func__
	<< ": pass proposer_rank = " << m->rank << ", prepare_pn = " << m->pn
	<< ": responsed_pn = " << responsed_pn
	<< ", accepted_pn = " << accepted_pn
	<< std::endl;
/*
      if (!accepted_pn) {
 	reply->responsed_pn = responsed_pn;
	reply->responsed_pn = m->pn; // use prepare pn if not accept any pn
					// first and only accept one pn
      } else {
	reply->pn = accepted_pn; // use accepted pn
      }
*/
      //ss << "phase 1: pass prepare pn = " << m->pn
	//<< " with accepted_pn = " << accepted_pn;
    } else { // current prepare pn is refused
      reply->refuse = 1;
      std::cout << "Accepter::" << __func__
	<< ": refuse proposer_rank = " << m->rank << ", prepare_pn = " << m->pn
	<< ": responsed_pn = " << responsed_pn
	<< ", accepted_pn = " << accepted_pn
	<< std::endl;
      //ss << "phase 1: refuse prepare pn = " << m->pn
	//<< " because responsed_pn = " << responsed_pn;
    }
    reply->responsed_pn = responsed_pn;
    reply->accepted_pn = accepted_pn;
    reply->reply_pn = m->pn;
    // todo select value from memory/storage
    reply->value = accepted_value;
    //std::cout << __func__ << ": msg type = "
//	<< message_type(reply->header.type) << std::endl;
    msgr->submit_message(m->fd, reply);
    return 0;
  }
  int process_accept_request(PaxosAcceptRequest *m) {
    //std::cout << "PaxosAcceptRequest = " << *m << std::endl;
    // answer prepare request with prepare response
    Message *req = create_message(PAXOS_ACCEPT_RESPONSE);   
    PaxosAcceptResponse *reply = (PaxosAcceptResponse *)req;
    reply->rank = rank;
    reply->epoll_fd = m->epoll_fd;
    reply->fd = m->fd;
    std::stringstream ss; 
    /*
     * If an acceptor receives a prepare request with number n greater
     * than that of any prepare request to which it has already responded,
     * then it responds to the request with a promise not to accept any more
     * proposals numbered less than n and with the highest-numbered proposal
     * (if any) that it has accepted.
     */
    if (m->accept_pn >= responsed_pn && m->accept_pn >= accepted_pn) {
      // accept this proposal
      reply->refuse = 0;
      
      // update accepted_pn/accepted_value once in a instance
      if (!accepted_pn) {
	accepted_pn = m->accept_pn;
	accepted_value = m->accept_value;	
      }
      std::cout << "Accepter::" << __func__
	<< ": accept proposer_rank = " << m->rank << ", accept_pn = " << m->accept_pn
	<< ": responsed_pn = " << responsed_pn
	<< ", accepted_pn = " << accepted_pn
	<< std::endl;
    } else {
      // refuse this proposal
      reply->refuse = 1;
      //reply->accepted_pn = accepted_pn;
      std::cout << "Accepter::" << __func__
	<< ": refuse proposer_rank = " << m->rank << ", accept_pn = " << m->accept_pn
	<< ": responsed_pn = " << responsed_pn
	<< ", accepted_pn = " << accepted_pn
	<< std::endl;
    }
    reply->responsed_pn = responsed_pn;
    reply->accepted_pn = accepted_pn;
    reply->reply_pn = m->accept_pn;
    msgr->submit_message(m->fd, reply);
    return 0;
  }
};

// act as proposer, accepter and learner
class PaxosMember {
public:
  Proposer proposer;
  Accepter accepter;
  uint64_t id; //instance id
  int rank;
  Messenger *msgr;
  Quorum *quorum;
  PaxosMember(Messenger *_msgr)
    : msgr(_msgr), proposer(_msgr), id(0), rank(-1),
      accepter(_msgr), quorum(NULL)
  {
    proposer.set_member(this);
    accepter.set_member(this);
  }
  int set_quorum(Quorum *q) { quorum = q; }
  int set_rank(int r) {
    rank = r;
    proposer.set_rank(r);
    accepter.set_rank(r);
  }
  int process_command(PaxosCommandRequest *req)
  {
    if (!req)
      return 0;
    int r = 0;
    req->decode_payload();
    if (req->name == "prepare") {
      uint64_t pn = strtoul(req->val.c_str(), NULL, 10);
      //proposer.set_rank(0); 
      pn = proposer.gen_pn2();
      //proposer.sent_pn = pn;

      proposer.broadcast_prepare_request(req->epoll_fd, pn);
      PaxosCommandResponse *reply = (PaxosCommandResponse *)
	create_message(PAXOS_COMMAND_RESPONSE);
      reply->rank = rank;
      reply->epoll_fd = req->epoll_fd;
      reply->fd = req->fd;
      reply->result = 0;
      msgr->submit_message(reply->fd, reply);
    }
  }
};

class Paxos : public MessengerUser {
public:
  Quorum quorum;
  PaxosMember member;
  //Messenger *msgr;
  Paxos(Messenger *_msgr, int type_id)
    : MessengerUser(_msgr, type_id), member(_msgr) {}
  int dispatch_message(Message *m)
  {
    switch(m->header.type) {
    case PAXOS_PREPARE_REQUEST:
    {
      PaxosPrepareRequest *req = (PaxosPrepareRequest *)m;
      return member.accepter.process_prepare_request(req);
    }
    case PAXOS_PREPARE_RESPONSE:
    {
      PaxosPrepareResponse *req = (PaxosPrepareResponse *)m;
      return member.proposer.process_prepare_response(req);
    }
    case PAXOS_ACCEPT_REQUEST:
    {
      PaxosAcceptRequest *req = (PaxosAcceptRequest *)m;
      return member.accepter.process_accept_request(req);
    }
    case PAXOS_ACCEPT_RESPONSE:
    {
      PaxosAcceptResponse *req = (PaxosAcceptResponse *)m;
      return member.proposer.process_accept_response(req);
    }
    case PAXOS_COMMAND_REQUEST:
    {
      PaxosCommandRequest *req = (PaxosCommandRequest *)m;
      return member.process_command(req);
    }
    default:
      std::cout << "unknown message" << std::endl;
      return 0;
    }
    return 0;
  }

  int construct_quorum() {
    Socket *addr0 = new Socket();   
    Socket *addr1 = new Socket();   
    Socket *addr2 = new Socket();   
    addr0->init(THISIP, PORT_0);
    addr1->init(THISIP, PORT_1);
    addr2->init(THISIP, PORT_2);
    quorum.insert_quorum(addr0);
    quorum.insert_quorum(addr1);
    quorum.insert_quorum(addr2);
  }

  int destruct_quorum() {
    std::set<Socket *>::iterator itr;
    std::set<Socket *> &q = quorum.get_quorum();
    for (itr = q.begin(); itr != q.end(); ++itr) {
      delete *itr;
    }
    quorum.clear_quorum();
    return 0;
  }

  int init(int r)
  {
    //std::cout << ": init rank = " << r << std::endl;
    msgr->add_messenger_user(this);
    construct_quorum();
    member.set_quorum(&quorum);
    member.set_rank(r);
    return 0;
  }

  int run()
  {
    msgr->run();
    return 0;
  }
};

#endif
