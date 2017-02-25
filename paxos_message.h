
#ifndef PAXOS_MESSAGE_H
#define PAXOS_MESSAGE_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <assert.h>
#include <malloc.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/access.hpp>

#define MSG_REQUEST 1
#define MSG_RESPONSE 2

#define MSG_TYPE_STEP 100

#define MSG_ID_PAXOS 1
#define MSG_ID_ELECT 2
#define MSG_ID_SYNC 3
#define MSG_ID_COMMAND 4

#define MSG_PAXOS (MSG_ID_PAXOS * MSG_TYPE_STEP)
#define MSG_ELECT (MSG_ID_ELECT * MSG_TYPE_STEP)
#define MSG_SYNC (MSG_ID_SYNC * MSG_TYPE_STEP)
#define MSG_COMMAND (MSG_ID_COMMAND * MSG_TYPE_STEP)

#define PAXOS_PREPARE_REQUEST (MSG_PAXOS + 1)
#define PAXOS_PREPARE_RESPONSE (MSG_PAXOS + 2)
#define PAXOS_ACCEPT_REQUEST (MSG_PAXOS + 3)
#define PAXOS_ACCEPT_RESPONSE (MSG_PAXOS + 4)
#define PAXOS_COMMAND_REQUEST (MSG_PAXOS + 5)
#define PAXOS_COMMAND_RESPONSE (MSG_PAXOS + 6)

#define COMMAND_REQUEST (MSG_COMMAND + 1)
#define COMMAND_RESPONSE (MSG_COMMAND + 2)

#define BOOST_ARCHIVE_LEN 48

typedef const unsigned int sver_t; // serilization verision type

int get_message_base_id(int itype);
int get_message_base_type(int itype);

const char *message_type(int type);

struct buff_pair {
  char *buf;
  uint64_t len;
};

class buffer_map {
  std::map<void *, uint64_t> data;
  uint64_t size;
  //char *seek_pos(uint64_t pos);
  //void append_item(void *buf, size_t len);
};

/*
char *buffer_map::seek_pos(uint64_t pos)
{
  assert(pos <= size);
  std::map<void *, uint64_t>::iterator itr = data.begin();
  uint64_t cur = 0;
  uint64_t next = 0;
  char *ptr = (char *)itr->first;
  uint64_t relative_off = 0;
  for (; itr != data.end(); itr++) {
    next += itr->second;
    if (next > pos) {
      relative_off = pos - cur;
      break;
    }
    cur = next;
    ptr = (char *)itr->first;
  }
  return ptr + relative_off; 
}

void buffer_map::append_item(void *buf, size_t len)
{
  data[buf] = len;
  size += len;
}
*/

struct msg_buf{
  char *header_buf;
  uint64_t header_len;
  char *payload_buf;
  uint64_t payload_len;
};

struct msg_header {
  int type;
  //uint64_t seq;
  uint64_t payload_len;
};

struct msg_counter {
  uint64_t sent;
};

class Message {
public:
  struct msg_header header;
  std::stringstream dss;
  char *payload_buf;
  int epoll_fd;
  int fd;
  int rank; // rank of paxos member, which proposer/accepter/learner send this message
  uint64_t sent;
  uint64_t got;
  bool header_wrote;
  Message(int _type)
  : payload_buf(NULL), sent(0), got(0), header_wrote(false), 
    epoll_fd(-1), fd(-1), rank(-1)
  {
    memset(&header, 0, sizeof(header));
    header.type = _type;
  }
  virtual ~Message()
  {
    if (payload_buf)
      delete [] payload_buf;
  };
  void set_paxos_rank(int r) { rank = r; }
  virtual void encode_payload() = 0;
  virtual void decode_payload() = 0;
};

class PaxosPrepareRequest : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosPrepareRequest &m);
public:
  uint64_t pn;
  PaxosPrepareRequest() : Message(PAXOS_PREPARE_REQUEST), pn(0)
  {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & rank;
    ar & pn;
  }
  virtual void decode_payload()
  {
    assert(payload_buf != NULL);
    //std::cout << "before setbuf dss.str() = " << dss.str() << std::endl;
    //std::cout << "payload_buf = " << payload_buf << std::endl;
    //dss->rdbuf()->pubsetbuf(payload_buf, strlen(payload_buf));
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

BOOST_CLASS_VERSION(PaxosPrepareRequest, 1)

/*
std::ostream& operator <<(std::ostream& out, const PaxosPrepareRequest &m)
{
  out << "(" << m.pn << "\")" << std::endl;
  return out;
}
*/

class PaxosPrepareResponse : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosPrepareResponse &m);
public:
  int refuse; //if refused by accepter, 1 means refuse, 0 means accept
  uint64_t responsed_pn; // pn of accepter saved
  uint64_t accepted_pn; // pn of accepter saved
  uint64_t reply_pn;	// pn of proposer send
  std::string value;
  PaxosPrepareResponse()
    : Message(PAXOS_PREPARE_RESPONSE), refuse(1),
      responsed_pn(0), accepted_pn(0), reply_pn(0)
  {
  }
  ~PaxosPrepareResponse() {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & rank;
    ar & refuse;
    ar & responsed_pn;
    ar & accepted_pn;
    ar & reply_pn;
    ar & value;
  }
  virtual void decode_payload()
  {
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

BOOST_CLASS_VERSION(PaxosPrepareResponse, 1)

class PaxosAcceptRequest : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosAcceptRequest &m);
public:
  uint64_t accept_pn;
  std::string accept_value;
  PaxosAcceptRequest() : Message(PAXOS_ACCEPT_REQUEST), accept_pn(0)
  {
  }
  ~PaxosAcceptRequest() {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & rank;
    ar & accept_pn;
    ar & accept_value;
  }
  virtual void decode_payload()
  {
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

class PaxosAcceptResponse : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosAcceptResponse &m);
public:
  int refuse; //if refused by accepter, 1 means refuse, 0 means accept
  uint64_t responsed_pn; // pn of accepter saved
  uint64_t accepted_pn; // pn of accepter saved
  uint64_t reply_pn;	// pn of propose send
  PaxosAcceptResponse()
    : Message(PAXOS_ACCEPT_RESPONSE), refuse(1),
      responsed_pn(0), accepted_pn(0), reply_pn(0)
  {
  }
  ~PaxosAcceptResponse() {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & rank;
    ar & refuse;
    ar & responsed_pn;
    ar & accepted_pn;
    ar & reply_pn;
  }
  virtual void decode_payload()
  {
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

class PaxosCommandRequest : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosCommandRequest &m);
public:
  std::string name;
  std::string val;
  PaxosCommandRequest() : Message(PAXOS_COMMAND_REQUEST)
  {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & name;
    ar & val;
  }
  virtual void decode_payload()
  {
    assert(payload_buf != NULL);
    //std::cout << "before setbuf dss.str() = " << dss.str() << std::endl;
    //std::cout << "payload_buf = " << payload_buf << std::endl;
    //dss->rdbuf()->pubsetbuf(payload_buf, strlen(payload_buf));
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

BOOST_CLASS_VERSION(PaxosCommandRequest, 1)

class PaxosCommandResponse : public Message {
  friend std::ostream& operator <<(std::ostream& out,
        const PaxosCommandResponse &m);
public:
  int result;
  PaxosCommandResponse() : Message(PAXOS_COMMAND_RESPONSE)
  {}
  template<class Archive>
  void serialize(Archive &ar, sver_t v)
  {
    ar & rank;
    ar & result;
  }
  virtual void decode_payload()
  {
    assert(payload_buf != NULL);
    //std::cout << "before setbuf dss.str() = " << dss.str() << std::endl;
    //std::cout << "payload_buf = " << payload_buf << std::endl;
    //dss->rdbuf()->pubsetbuf(payload_buf, strlen(payload_buf));
    dss.rdbuf()->pubsetbuf(payload_buf, header.payload_len);
    //std::cout << "dss.str() = " << dss.str() << std::endl;
    boost::archive::binary_iarchive ia(dss);
    this->serialize(ia, 1);
  }
  virtual void encode_payload()
  {
    boost::archive::binary_oarchive oa(dss);
    this->serialize(oa, 1);
    header.payload_len = dss.str().size();
  }
};

BOOST_CLASS_VERSION(PaxosCommandResponse, 1)

#endif
