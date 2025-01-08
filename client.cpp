#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <cassert>
#include <fcntl.h>

const size_t k_max_msg = 4096;

static void set_Fd_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int rv = fcntl(fd, F_SETFL, flags);
  assert(rv == 0);
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

static int32_t query(int fd, const char *text)
{
  uint32_t len = (uint32_t)strlen(text);

  if (len > k_max_msg)
  {
    return -1;
  }

  char wbuf[4 + k_max_msg];

  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], text, len);

  if (int32_t err = write_full(fd, wbuf, 4 + len))
  {
    return err;
  }

  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(fd, rbuf, 4);
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
  memcpy(&len, rbuf, 4);

  if (len > k_max_msg)
  {
    std::cerr << "too long" << std::endl;
    return -1;
  }

  err = read_full(fd, &rbuf[4], len);
  if (err)
  {
    std::cerr << "read() error" << std::endl;
    return err;
  }
  // do something
  rbuf[4 + len] = '\0';
  printf("server says: %s\n", &rbuf[4]);

  return 0;
}

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

  int32_t err = query(fd, "hello1");
  if (err)
  {
    goto L_DONE;
  }
  err = query(fd, "hello2");
  if (err)
  {
    goto L_DONE;
  }
  err = query(fd, "hello3");
  if (err)
  {
    goto L_DONE;
  }

L_DONE:
  close(fd);
  return 0;
}