#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <cassert>
#include <fcntl.h>
#include <vector>

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

static int32_t send_req(int fd, const char *text, size_t len)
{

  if (len > k_max_msg)
  {
    return -1;
  }

  char wbuf[4 + k_max_msg];

  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], text, len);

  return write_full(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd)
{

  uint32_t len = 0;
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

  // multiple pipelined requests
  std::vector<std::string> query_list = {
      "hello1",
      "hello2",
      "hello3",
      // a large message requires multiple event loop iterations
      std::string(k_max_msg, 'z'),
      "hello5",
  };
  for (const std::string &s : query_list)
  {
    int32_t err = send_req(fd, s.data(), s.size());
    if (err)
    {
      goto L_DONE;
    }
  }
  for (size_t i = 0; i < query_list.size(); ++i)
  {
    int32_t err = read_res(fd);
    if (err)
    {
      goto L_DONE;
    }
  }

L_DONE:
  close(fd);
  return 0;
}
