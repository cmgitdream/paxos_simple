
#include <iostream>
#include "paxos_message.h"


int get_message_base_id(int itype)
{
  return itype / MSG_TYPE_STEP;
}

int get_message_base_type(int itype)
{
  return itype / MSG_TYPE_STEP * MSG_TYPE_STEP;
}

const char *message_type(int type)
{
  switch(type) {
  case PAXOS_PREPARE_REQUEST: return "PAXOS_PREPARE_REQUEST";
  case PAXOS_PREPARE_RESPONSE: return "PAXOS_PREPARE_RESPONSE";
  case PAXOS_ACCEPT_REQUEST: return "PAXOS_ACCEPT_REQUEST";
  case PAXOS_ACCEPT_RESPONSE: return "PAXOS_ACCEPT_RESPONSE";
  case PAXOS_COMMAND_REQUEST: return "PAXOS_COMMAND_REQUEST";
  case PAXOS_COMMAND_RESPONSE: return "PAXOS_COMMAND_RESPONSE";
  default:
    return "WRONG_TYPE";
  }
  return "WRONG_TYPE";
}

std::ostream& operator <<(std::ostream& out, const PaxosPrepareRequest &m)
{
  out << "(" << m.pn << ")";
  return out;
}

std::ostream& operator <<(std::ostream& out, const PaxosPrepareResponse &m)
{
  out << "(" << (m.refuse != 0 ? "refuse" : "pass" )
	<< ", " << m.responsed_pn  << ", " << m.accepted_pn << ", \"" << m.value << "\")";
  return out;
}

std::ostream& operator <<(std::ostream& out, const PaxosAcceptRequest &m)
{
  out << "(" << ", " << m.accept_pn << ", \"" << m.accept_value << "\")";
  return out;
}

std::ostream& operator <<(std::ostream& out, const PaxosAcceptResponse &m)
{
  out << "(" << (m.refuse != 0 ? "refuse" : "pass" )
	<< ", " << m.responsed_pn << ", \"" << m.accepted_pn << "\")";
  return out;
}
