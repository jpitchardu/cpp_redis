#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <string.h>

static void do_something(int connfd)
{
  char rbuf[64] = {};

  ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);

  if (n < 0)
  {
    std::cout << "read() failed" << std::endl;
    return;
  }

  printf("read %zd bytes: %.*s\n", n, (int)n, rbuf);

  char wbuf[] = "world";
  write(connfd, wbuf, strlen(wbuf));
}

int main()
{
  int val = 1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};

  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0);

  int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));

  if (rv)
  {
    std::cerr << "bind() failed" << std::endl;
    return 1;
  }

  int rv2 = listen(fd, SOMAXCONN);

  if (rv2)
  {
    std::cerr << "listen() failed" << std::endl;
    return 1;
  }

  while (true)
  {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);

    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

    if (connfd < 0)
    {
      std::cerr << "accept() failed" << std::endl;
      continue;
    }

    do_something(connfd);
    close(connfd);
  }

  return 0;
}
