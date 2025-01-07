#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <cassert>

const size_t k_max_msg = 4096;

static int32_t read_full(int fd, char *buf, size_t n)
{
  while (n > 0)
  {
    ssize_t rv = read(fd, buf, n);

    if (rv <= 0)
    {
      return -1;
    }

    assert((size_t)rv <= n);

    n -= (size_t)rv;
    buf += rv;
  }

  return 0;
}

static int32_t write_full(int fd, const char *buf, size_t n)
{
  while (n > 0)
  {
    ssize_t rv = write(fd, buf, n);

    if (rv <= 0)
    {
      return -1;
    }

    assert((size_t)rv <= n);

    n -= (size_t)rv;
    buf += rv;
  }

  return 0;
}

static int32_t one_request(int connfd)
{
  char rbuf[4 + k_max_msg + 1];

  errno = 0;

  int32_t err = read_full(connfd, rbuf, 4);

  if (err)
  {
    if (errno == 0)
    {
      std::cerr << "EOF" << std::endl;
    }
    else
    {
      std::cerr << "read_full() failed" << std::endl;
    }
    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);

  if (len > k_max_msg)
  {
    std::cerr << "too long" << std::endl;
    return -1;
  }

  // Read the message
  err = read_full(connfd, &rbuf[4], len);
  if (err)
  {
    std::cerr << "read_full() failed" << std::endl;
    return err;
  }

  rbuf[4 + len] = '\0';
  printf("client says: %s\n", &rbuf[4]);

  // Reply

  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);

  return write_full(connfd, wbuf, 4 + len);
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

    while (true)
    {
      int32_t err = one_request(connfd);
      if (err)
      {
        break;
      }
    }

    close(connfd);
  }

  return 0;
}
