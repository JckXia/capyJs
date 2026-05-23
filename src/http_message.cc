#include "http_message.h"
#include "client_state.h"
#include "server.h"
#include <cstdlib>

int ResponseObject::build_headers(char *header_buf, size_t header_len,
                                  ResponseObject *res) {
  if (header_buf == nullptr || header_len == 0) {
    return -1;
  }

  if (res->header_size == 0) {
    int ret = snprintf(header_buf, header_len,
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n",
                       res->response_len);
    return ret;
  }

  size_t offset = 0;

  // Status line + standard headers (no terminating \r\n yet)
  offset += snprintf(header_buf, header_len,
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: keep-alive\r\n",
                     res->response_len);

  // Append custom headers
  for (auto &[header_key, header_value] : res->headers) {
    offset += snprintf(header_buf + offset, header_len - offset, "%s: %s\r\n",
                       header_key, header_value);
  }

  // NOW end the headers
  offset += snprintf(header_buf + offset, header_len - offset, "\r\n");
  return offset;
}

void ResponseObject::send() {
  if (!guard->alive) {
    guard->release();
    delete this->req;
    delete this;
    return;
  }
  ClientState *client_state = (ClientState *)cli->data;

  if (client_state->closing == true) {
    guard->release();
    delete this->req;
    delete this;
    return;
  }
  size_t header_len = header_size + 256;
  char header_buf[header_len]; // 256 default headers
  header_len = build_headers(header_buf, header_len, this);
  uv_write_t *write_handle = &client_state->write_handle;
  write_handle->data = client_state;


  delete this->req; // Destroys the linked request object
  if (is_static) {
    uv_buf_t bufs[2] = {uv_buf_init(header_buf, header_len),
                        uv_buf_init(response_buffer, response_len)};

    int rc;
    if ((rc = uv_write(write_handle, cli, bufs, 2, Server::on_static_write_cb)) < 0) {
      std::cout << "Write to socket failed! " << uv_strerror(rc) << std::endl;
    }
    guard->release();
    delete (this);
  } else {
    for (auto &[k, v] : headers) { free((void*)k); free((void*)v); }
    headers.clear();
    client_state->pending_write_buffer = (char *)malloc(response_len);

    memcpy(client_state->pending_write_buffer, response_buffer, response_len);
    free(response_buffer);
    uv_buf_t bufs[2] = {
        uv_buf_init(header_buf, header_len),
        uv_buf_init(client_state->pending_write_buffer, response_len)};

    int rc;
    if ((rc = uv_write(write_handle, cli, bufs, 2, Server::on_write_cb)) < 0) {
      std::cout << "Write to socket failed! " << uv_strerror(rc) << std::endl;
      guard->release();
      delete (this);
      return;
    }
    guard->release();
    delete (this);
  }
}

void ResponseObject::on_shutdown_cb(uv_shutdown_t* req, int status) {
  ClientState *client_state = (ClientState *)req->data;
  client_state->write_in_flight = false;
  uv_read_start((uv_stream_t *)&client_state->socket,
                Server::on_alloc_buffer_cb, Server::on_read_cb);
  free(req);
}

static void on_abort_shutdown_cb(uv_shutdown_t *req, int status) {
  ClientState *client_state = (ClientState *)req->data;
  free(req);
  uv_close((uv_handle_t *)&client_state->socket, Server::on_client_closed_cb);
}

static void on_abort_write_cb(uv_write_t *req, int status) {
  uv_shutdown_t *sr = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
  sr->data = req->data;
  uv_shutdown(sr, req->handle, on_abort_shutdown_cb);
}

void ResponseObject::abort() {
  if (!guard->alive) {
    guard->release();
    delete this;
    return;
  }
  ClientState *client_state = (ClientState *)cli->data;
  if (client_state->closing == true) {
    guard->release();
    delete this;
    return;
  }

  static const char resp[] =
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
  uv_write_t *write_handle = &client_state->write_handle;
  write_handle->data = client_state;
  uv_buf_t buf = uv_buf_init(const_cast<char *>(resp), sizeof(resp) - 1);
  int rc;
  if ((rc = uv_write(write_handle, cli, &buf, 1, on_abort_write_cb)) < 0) {
    std::cout << "abort write failed: " << uv_strerror(rc) << std::endl;
    uv_close((uv_handle_t *)&client_state->socket, Server::on_client_closed_cb);
  }

  guard->release();
  delete this;
}