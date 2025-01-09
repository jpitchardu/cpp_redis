#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <cassert>
#include <vector>
#include <poll.h>
#include <fcntl.h>

const size_t k_max_msg = 4096;

static void set_Fd_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int rv = fcntl(fd, F_SETFL, flags);
  assert(rv == 0);
}

struct Conn
{
  int fd = -1;

  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
};

static Conn *handle_accept(int fd)
{
  struct sockaddr_in addr = {};
  socklen_t len = sizeof(addr);
  int conn_fd = accept(fd, (sockaddr *)&addr, &len);

  if (conn_fd < 0)
  {
    return NULL;
  }

  uint32_t ip = addr.sin_addr.s_addr;
  printf("new client from %u.%u.%u.%u:%u\n", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24, ntohs(addr.sin_port));

  set_Fd_nonblock(conn_fd);

  Conn *conn = new Conn();
  conn->fd = conn_fd;
  conn->want_read = true;
  return conn;
}

static bool try_one_request(Conn *conn)
{
  if (conn->incoming.size() < 4)
  {
    return false;
  }

  uint32_t len = 0;
  memcpy(&len, &conn->incoming[0], 4);

  if (len > k_max_msg)
  {
    conn->want_close = true;
    return false;
  }

  if (conn->incoming.size() < len + 4)
  {
    return false;
  }

  const uint8_t *data = &conn->incoming[4];

  // got one request, do some application logic
  printf("client says: len:%d data:%.*s\n", len, len < 100 ? len : 100, data);

  conn->outgoing.insert(conn->outgoing.end(), (uint8_t *)&len, (uint8_t *)&len + 4);
  conn->outgoing.insert(conn->outgoing.end(), data, data + len);

  conn->incoming.erase(conn->incoming.begin(), conn->incoming.begin() + 4 + len);

  return true;
}

static void handle_write(Conn *conn)
{
  assert(!conn->outgoing.empty());
  ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

  if (rv < 0)
  {
    conn->want_close = true;
    return;
  }

  conn->outgoing.erase(conn->outgoing.begin(), conn->outgoing.begin() + rv);

  if (conn->outgoing.size() == 0)
  {
    conn->want_read = true;
    conn->want_write = false;
  }
}

static void handle_read(Conn *conn)
{
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));

  if (rv < 0 && errno == EAGAIN)
  {
    return; // actually not ready
  }

  if (rv < 0)
  {
    std::cerr << "read() failed" << std::endl;
    conn->want_close = true;
    return;
  }

  if (rv == 0)
  {

    printf(conn->incoming.size() == 0 ? "client closed" : "unexpected EOF");
    conn->want_close = true;
    return; // want close
  }

  conn->incoming.insert(conn->incoming.end(), buf, buf + rv);

  while (try_one_request(conn))
  {
  }

  if (conn->outgoing.size() > 0)
  {
    conn->want_read = false;
    conn->want_write = true;
    return handle_write(conn);
  }
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

  rv = listen(fd, SOMAXCONN);
  if (rv)
  {
    std::cerr << "listen() failed" << std::endl;
    return 1;
  }

  // a map of all client connections, keyed by fd
  std::vector<Conn *> fd2Conn;

  std::vector<struct pollfd> poll_args;

  while (true)
  {
    poll_args.clear();

    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn *conn : fd2Conn)
    {
      if (!conn)
      {
        continue;
      }

      struct pollfd pfd = {conn->fd, POLLERR, 0};

      if (conn->want_read)
      {
        pfd.events |= POLLIN;
      }
      if (conn->want_write)
      {
        pfd.events |= POLLOUT;
      }

      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR)
    {
      continue;
    }
    if (rv < 0)
    {
      std::cerr << "poll() failed" << std::endl;
      return 1;
    }

    if (poll_args[0].revents)
    {
      if (Conn *conn = handle_accept(fd))
      {
        if (fd2Conn.size() <= (size_t)conn->fd)
        {
          fd2Conn.resize((size_t)conn->fd + 1);
        }
        fd2Conn[conn->fd] = conn;
      }
    }

    for (size_t i = 1; i < poll_args.size(); ++i)
    {
      uint32_t ready = poll_args[i].revents;
      Conn *conn = fd2Conn[poll_args[i].fd];

      if (ready & POLLIN)
      {
        handle_read(conn);
      }
      if (ready & POLLOUT)
      {
        handle_write(conn);
      }

      if ((ready & POLLERR) || conn->want_close)
      {
        (void)close(conn->fd);
        fd2Conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }
}
