
#include "paxos_common.h"
#include <iostream>
using std::cout;
using std::endl;

// port = 10010, 10020, 10030 ...

void usage()
{
  std::cout << "./paxos_main <ip_of_this_host> <port> <rank> [mkfs]\n" << std::endl;
}


int main(int argc, char *argv[])
{
  if (argc < 4) {
    usage();
    return -1;
  }
  char *ip = argv[1];
  uint16_t port = strtoul(argv[2], NULL, 10);
  int rank = atoi(argv[3]);
  if (argc == 4) {
    Messenger msgr;
    msgr.init(ip, port);
    Paxos paxos(&msgr, MSG_ID_PAXOS);
    paxos.init(rank, ip);
    cout << "runing" << std::endl;
    msgr.run();
  } else if (argc == 5 && strcmp(argv[4], "mkfs") == 0) {
    cout << "mkfs..." << endl;
    Paxos paxos(NULL, MSG_ID_PAXOS);
    paxos.mkfs(rank);
  }
  return 0;
}
