

#include "paxos_common.h"
#include <iostream>
using std::cout;
using std::endl;

// port = 10010, 10020, 10030 ...

void usage()
{
  std::cout << "./paxos_main <port> <rank>\n" << std::endl;
}


int main(int argc, char *argv[])
{
  //NetPeer proposer("proposer", "127.0.0.1", PROPOSER_PORT);
  //proposer.init();
  Messenger msgr;
  //msgr.init("127.0.0.1", 10010);
  if (argc < 3) {
    usage();
    return -1;
  }
  uint16_t port = strtoul(argv[1], NULL, 10);
  int rank = atoi(argv[2]);
  cout << "rank = " << rank << endl;
  msgr.init("10.0.11.212", port);
  Paxos paxos(&msgr, MSG_ID_PAXOS);
  paxos.init(rank);
  msgr.run();
  cout << " success" << std::endl;
  return 0;

}
