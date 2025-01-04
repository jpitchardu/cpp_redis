#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

int main()
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr = {};

  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
  int rv = connect(fd, (const sockaddr *)&addr, sizeof(addr));

  if (rv)
  {
    std::cerr << "connect() failed" << std::endl;
    return 1;
  }

  char wbuf[] = "hello";
  write(fd, wbuf, strlen(wbuf));

  char rbuf[64] = {};
  read(fd, rbuf, sizeof(rbuf) - 1);

  std::cout << rbuf << std::endl;

  close(fd);
}