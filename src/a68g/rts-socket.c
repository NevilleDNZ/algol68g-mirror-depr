//! @file rts-socket.c
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

//! @section Synopsis
//!
//! Low-level socket routines. 

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-transput.h"

#if defined (BUILD_HTTP)

#define PROTOCOL "tcp"
#define SERVICE "http"

#define CONTENT_BUFFER_SIZE (64 * KILOBYTE)
#define TIMEOUT_INTERVAL 15

#if defined (BUILD_UNIX)

//! @brief Send GET request to server and yield answer (TCP/HTTP only).

void genie_http_content (NODE_T * p)
{
  A68_REF path_string, domain_string, content_string;
  A68_INT port_number;
  int socket_id, conn, k;
  fd_set set;
  struct timeval a68_timeout;
  struct servent *service_address;
  struct hostent *host_address;
  struct protoent *protocol;
  struct sockaddr_in socket_address;
  char buffer[CONTENT_BUFFER_SIZE];
  errno = 0;
// Pop arguments.
  POP_OBJECT (p, &port_number, A68_INT);
  CHECK_INIT (p, INITIALISED (&port_number), M_INT);
  POP_REF (p, &path_string);
  CHECK_INIT (p, INITIALISED (&path_string), M_STRING);
  POP_REF (p, &domain_string);
  CHECK_INIT (p, INITIALISED (&domain_string), M_STRING);
  POP_REF (p, &content_string);
  CHECK_REF (p, content_string, M_REF_STRING);
  *DEREF (A68_REF, &content_string) = empty_string (p);
// Reset buffers.
  reset_transput_buffer (DOMAIN_BUFFER);
  reset_transput_buffer (PATH_BUFFER);
  reset_transput_buffer (REQUEST_BUFFER);
  reset_transput_buffer (CONTENT_BUFFER);
  add_a_string_transput_buffer (p, DOMAIN_BUFFER, (BYTE_T *) & domain_string);
  add_a_string_transput_buffer (p, PATH_BUFFER, (BYTE_T *) & path_string);
// Make request.
  add_string_transput_buffer (p, REQUEST_BUFFER, "GET ");
  add_string_transput_buffer (p, REQUEST_BUFFER, get_transput_buffer (PATH_BUFFER));
  add_string_transput_buffer (p, REQUEST_BUFFER, " HTTP/1.0\n\n");
// Connect to host.
  FILL (&socket_address, 0, (int) sizeof (socket_address));
  SIN_FAMILY (&socket_address) = AF_INET;
  service_address = getservbyname (SERVICE, PROTOCOL);
  if (service_address == NULL) {
    PUSH_VALUE (p, 1, A68_INT);
    return;
  }
  if (VALUE (&port_number) == 0) {
    SIN_PORT (&socket_address) = (uint16_t) (S_PORT (service_address));
  } else {
    SIN_PORT (&socket_address) = (uint16_t) (htons ((uint16_t) (VALUE (&port_number))));
    if (SIN_PORT (&socket_address) == 0) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      return;
    }
  }
  host_address = gethostbyname (get_transput_buffer (DOMAIN_BUFFER));
  if (host_address == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  COPY (&SIN_ADDR (&socket_address), H_ADDR (host_address), H_LENGTH (host_address));
  protocol = getprotobyname (PROTOCOL);
  if (protocol == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  socket_id = socket (PF_INET, SOCK_STREAM, P_PROTO (protocol));
  if (socket_id < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  conn = connect (socket_id, (const struct sockaddr *) &socket_address, (socklen_t) SIZE_ALIGNED (socket_address));
  if (conn < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Read from host.
  WRITE (socket_id, get_transput_buffer (REQUEST_BUFFER));
  if (errno != 0) {
    PUSH_VALUE (p, errno, A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Initialise file descriptor set.
  FD_ZERO (&set);
  FD_SET (socket_id, &set);
// Initialise the a68_timeout data structure.
  TV_SEC (&a68_timeout) = TIMEOUT_INTERVAL;
  TV_USEC (&a68_timeout) = 0;
// Block until server replies or a68_timeout blows up.
  switch (select (FD_SETSIZE, &set, NULL, NULL, &a68_timeout)) {
  case 0:
    {
      errno = ETIMEDOUT;
      PUSH_VALUE (p, errno, A68_INT);
      ASSERT (close (socket_id) == 0);
      return;
    }
  case -1:
    {
      PUSH_VALUE (p, errno, A68_INT);
      ASSERT (close (socket_id) == 0);
      return;
    }
  case 1:
    {
      break;
    }
  default:
    {
      ABEND (A68_TRUE, ERROR_ACTION, __func__);
    }
  }
  while ((k = (int) io_read (socket_id, &buffer, (CONTENT_BUFFER_SIZE - 1))) > 0) {
    add_chars_transput_buffer (p, CONTENT_BUFFER, k, buffer);
  }
  if (k < 0 || errno != 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Convert string.
  *DEREF (A68_REF, &content_string) = c_to_a_string (p, get_transput_buffer (CONTENT_BUFFER), get_transput_buffer_index (CONTENT_BUFFER));
  ASSERT (close (socket_id) == 0);
  PUSH_VALUE (p, errno, A68_INT);
}

//! @brief Send request to server and yield answer (TCP only).

void genie_tcp_request (NODE_T * p)
{
  A68_REF path_string, domain_string, content_string;
  A68_INT port_number;
  int socket_id, conn, k;
  fd_set set;
  struct timeval a68_timeout;
  struct servent *service_address;
  struct hostent *host_address;
  struct protoent *protocol;
  struct sockaddr_in socket_address;
  char buffer[CONTENT_BUFFER_SIZE];
  errno = 0;
// Pop arguments.
  POP_OBJECT (p, &port_number, A68_INT);
  CHECK_INIT (p, INITIALISED (&port_number), M_INT);
  POP_REF (p, &path_string);
  CHECK_INIT (p, INITIALISED (&path_string), M_STRING);
  POP_REF (p, &domain_string);
  CHECK_INIT (p, INITIALISED (&domain_string), M_STRING);
  POP_REF (p, &content_string);
  CHECK_REF (p, content_string, M_REF_STRING);
  *DEREF (A68_REF, &content_string) = empty_string (p);
// Reset buffers.
  reset_transput_buffer (DOMAIN_BUFFER);
  reset_transput_buffer (PATH_BUFFER);
  reset_transput_buffer (REQUEST_BUFFER);
  reset_transput_buffer (CONTENT_BUFFER);
  add_a_string_transput_buffer (p, DOMAIN_BUFFER, (BYTE_T *) & domain_string);
  add_a_string_transput_buffer (p, PATH_BUFFER, (BYTE_T *) & path_string);
// Make request.
  add_string_transput_buffer (p, REQUEST_BUFFER, get_transput_buffer (PATH_BUFFER));
// Connect to host.
  FILL (&socket_address, 0, (int) sizeof (socket_address));
  SIN_FAMILY (&socket_address) = AF_INET;
  service_address = getservbyname (SERVICE, PROTOCOL);
  if (service_address == NULL) {
    PUSH_VALUE (p, 1, A68_INT);
    return;
  }
  if (VALUE (&port_number) == 0) {
    SIN_PORT (&socket_address) = (uint16_t) (S_PORT (service_address));
  } else {
    SIN_PORT (&socket_address) = (uint16_t) (htons ((uint16_t) (VALUE (&port_number))));
    if (SIN_PORT (&socket_address) == 0) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      return;
    }
  }
  host_address = gethostbyname (get_transput_buffer (DOMAIN_BUFFER));
  if (host_address == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  COPY (&SIN_ADDR (&socket_address), H_ADDR (host_address), H_LENGTH (host_address));
  protocol = getprotobyname (PROTOCOL);
  if (protocol == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  socket_id = socket (PF_INET, SOCK_STREAM, P_PROTO (protocol));
  if (socket_id < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    return;
  }
  conn = connect (socket_id, (const struct sockaddr *) &socket_address, (socklen_t) SIZE_ALIGNED (socket_address));
  if (conn < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Read from host.
  WRITE (socket_id, get_transput_buffer (REQUEST_BUFFER));
  if (errno != 0) {
    PUSH_VALUE (p, errno, A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Initialise file descriptor set.
  FD_ZERO (&set);
  FD_SET (socket_id, &set);
// Initialise the a68_timeout data structure.
  TV_SEC (&a68_timeout) = TIMEOUT_INTERVAL;
  TV_USEC (&a68_timeout) = 0;
// Block until server replies or a68_timeout blows up.
  switch (select (FD_SETSIZE, &set, NULL, NULL, &a68_timeout)) {
  case 0:
    {
      errno = ETIMEDOUT;
      PUSH_VALUE (p, errno, A68_INT);
      ASSERT (close (socket_id) == 0);
      return;
    }
  case -1:
    {
      PUSH_VALUE (p, errno, A68_INT);
      ASSERT (close (socket_id) == 0);
      return;
    }
  case 1:
    {
      break;
    }
  default:
    {
      ABEND (A68_TRUE, ERROR_ACTION, __func__);
    }
  }
  while ((k = (int) io_read (socket_id, &buffer, (CONTENT_BUFFER_SIZE - 1))) > 0) {
    add_chars_transput_buffer (p, CONTENT_BUFFER, k, buffer);
  }
  if (k < 0 || errno != 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    ASSERT (close (socket_id) == 0);
    return;
  }
// Convert string.
  *DEREF (A68_REF, &content_string) = c_to_a_string (p, get_transput_buffer (CONTENT_BUFFER), get_transput_buffer_index (CONTENT_BUFFER));
  ASSERT (close (socket_id) == 0);
  PUSH_VALUE (p, errno, A68_INT);
}

#endif

#if defined (BUILD_WIN32)

#if defined (HAVE_WINSOCK_H)
#include <winsock.h>
#endif
typedef int socklen_t;

//! @brief Send GET request to server and yield answer (TCP/HTTP only).

void genie_http_content (NODE_T * p)
{
  WSADATA wsa_data;
  A68_REF path_string, domain_string, content_string;
  A68_INT port_number;
  int socket_id, conn, k, rc, len, sent;
  struct servent *service_address;
  struct hostent *host_address;
  struct protoent *protocol;
  struct sockaddr_in socket_address;
  char buffer[CONTENT_BUFFER_SIZE];
  char *str;
  errno = 0;
// Pop arguments.
  POP_OBJECT (p, &port_number, A68_INT);
  CHECK_INIT (p, INITIALISED (&port_number), M_INT);
  POP_REF (p, &path_string);
  CHECK_INIT (p, INITIALISED (&path_string), M_STRING);
  POP_REF (p, &domain_string);
  CHECK_INIT (p, INITIALISED (&domain_string), M_STRING);
  POP_REF (p, &content_string);
  CHECK_REF (p, content_string, M_REF_STRING);
  *DEREF (A68_REF, &content_string) = empty_string (p);
// Reset buffers.
  reset_transput_buffer (DOMAIN_BUFFER);
  reset_transput_buffer (PATH_BUFFER);
  reset_transput_buffer (REQUEST_BUFFER);
  reset_transput_buffer (CONTENT_BUFFER);
  add_a_string_transput_buffer (p, DOMAIN_BUFFER, (BYTE_T *) & domain_string);
  add_a_string_transput_buffer (p, PATH_BUFFER, (BYTE_T *) & path_string);
// Make request.
  add_string_transput_buffer (p, REQUEST_BUFFER, "GET ");
  add_string_transput_buffer (p, REQUEST_BUFFER, get_transput_buffer (PATH_BUFFER));
  add_string_transput_buffer (p, REQUEST_BUFFER, " HTTP/1.0\n\n");
// Connect to host.
  if (WSAStartup (MAKEWORD (1, 1), &wsa_data) != NO_ERROR) {
    PUSH_VALUE (p, 1, A68_INT);
    return;
  }
  FILL (&socket_address, 0, (int) sizeof (socket_address));
  SIN_FAMILY (&socket_address) = AF_INET;
  service_address = getservbyname (SERVICE, PROTOCOL);
  if (service_address == NULL) {
    PUSH_VALUE (p, 1, A68_INT);
    WSACleanup ();
    return;
  }
  if (VALUE (&port_number) == 0) {
    SIN_PORT (&socket_address) = (uint16_t) (S_PORT (service_address));
  } else {
    SIN_PORT (&socket_address) = (uint16_t) (htons ((uint16_t) (VALUE (&port_number))));
    if (SIN_PORT (&socket_address) == 0) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      WSACleanup ();
      return;
    }
  }
  host_address = gethostbyname (get_transput_buffer (DOMAIN_BUFFER));
  if (host_address == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  COPY (&SIN_ADDR (&socket_address), H_ADDR (host_address), H_LENGTH (host_address));
  protocol = getprotobyname (PROTOCOL);
  if (protocol == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  socket_id = socket (PF_INET, SOCK_STREAM, P_PROTO (protocol));
  if (socket_id < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  conn = connect (socket_id, (const struct sockaddr *) &socket_address, (socklen_t) SIZE_ALIGNED (socket_address));
  if (conn < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
// Send request to host.
  str = get_transput_buffer (REQUEST_BUFFER);
  len = (int) strlen (str);
  sent = 0;
  while (sent < len) {
    rc = send (socket_id, &str[sent], len - sent, 0);
    if (rc == SOCKET_ERROR) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      WSACleanup ();
    }
    sent += rc;
  }
// Receive data from host.
  while ((k = (int) recv (socket_id, (char *) &buffer, (CONTENT_BUFFER_SIZE - 1), 0)) > 0) {
    add_chars_transput_buffer (p, CONTENT_BUFFER, k, buffer);
  }
  if (k < 0 || errno != 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
// Convert string.
  *DEREF (A68_REF, &content_string) = c_to_a_string (p, get_transput_buffer (CONTENT_BUFFER), get_transput_buffer_index (CONTENT_BUFFER));
  if (k != 0) {
// Not gracefully closed by recv ().
    ASSERT (close (socket_id) == 0);
  }
  PUSH_VALUE (p, errno, A68_INT);
  WSACleanup ();
}

//! @brief Send request to server and yield answer (TCP only).

void genie_tcp_request (NODE_T * p)
{
  WSADATA wsa_data;
  A68_REF path_string, domain_string, content_string;
  A68_INT port_number;
  int socket_id, conn, k, rc, len, sent;
  struct servent *service_address;
  struct hostent *host_address;
  struct protoent *protocol;
  struct sockaddr_in socket_address;
  char buffer[CONTENT_BUFFER_SIZE];
  char *str;
  errno = 0;
// Pop arguments.
  POP_OBJECT (p, &port_number, A68_INT);
  CHECK_INIT (p, INITIALISED (&port_number), M_INT);
  POP_REF (p, &path_string);
  CHECK_INIT (p, INITIALISED (&path_string), M_STRING);
  POP_REF (p, &domain_string);
  CHECK_INIT (p, INITIALISED (&domain_string), M_STRING);
  POP_REF (p, &content_string);
  CHECK_REF (p, content_string, M_REF_STRING);
  *DEREF (A68_REF, &content_string) = empty_string (p);
// Reset buffers.
  reset_transput_buffer (DOMAIN_BUFFER);
  reset_transput_buffer (PATH_BUFFER);
  reset_transput_buffer (REQUEST_BUFFER);
  reset_transput_buffer (CONTENT_BUFFER);
  add_a_string_transput_buffer (p, DOMAIN_BUFFER, (BYTE_T *) & domain_string);
  add_a_string_transput_buffer (p, PATH_BUFFER, (BYTE_T *) & path_string);
// Make request.
  add_string_transput_buffer (p, REQUEST_BUFFER, get_transput_buffer (PATH_BUFFER));
// Connect to host.
  if (WSAStartup (MAKEWORD (1, 1), &wsa_data) != NO_ERROR) {
    PUSH_VALUE (p, 1, A68_INT);
    return;
  }
  FILL (&socket_address, 0, (int) sizeof (socket_address));
  SIN_FAMILY (&socket_address) = AF_INET;
  service_address = getservbyname (SERVICE, PROTOCOL);
  if (service_address == NULL) {
    PUSH_VALUE (p, 1, A68_INT);
    WSACleanup ();
    return;
  }
  if (VALUE (&port_number) == 0) {
    SIN_PORT (&socket_address) = (uint16_t) (S_PORT (service_address));
  } else {
    SIN_PORT (&socket_address) = (uint16_t) (htons ((uint16_t) (VALUE (&port_number))));
    if (SIN_PORT (&socket_address) == 0) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      WSACleanup ();
      return;
    }
  }
  host_address = gethostbyname (get_transput_buffer (DOMAIN_BUFFER));
  if (host_address == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  COPY (&SIN_ADDR (&socket_address), H_ADDR (host_address), H_LENGTH (host_address));
  protocol = getprotobyname (PROTOCOL);
  if (protocol == NULL) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  socket_id = socket (PF_INET, SOCK_STREAM, P_PROTO (protocol));
  if (socket_id < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
  conn = connect (socket_id, (const struct sockaddr *) &socket_address, (socklen_t) SIZE_ALIGNED (socket_address));
  if (conn < 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
// Send request to host.
  str = get_transput_buffer (REQUEST_BUFFER);
  len = (int) strlen (str);
  sent = 0;
  while (sent < len) {
    rc = send (socket_id, &str[sent], len - sent, 0);
    if (rc == SOCKET_ERROR) {
      PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
      WSACleanup ();
    }
    sent += rc;
  }
// Receive data from host.
  while ((k = (int) recv (socket_id, (char *) &buffer, (CONTENT_BUFFER_SIZE - 1), 0)) > 0) {
    add_chars_transput_buffer (p, CONTENT_BUFFER, k, buffer);
  }
  if (k < 0 || errno != 0) {
    PUSH_VALUE (p, (errno == 0 ? 1 : errno), A68_INT);
    WSACleanup ();
    return;
  }
// Convert string.
  *DEREF (A68_REF, &content_string) = c_to_a_string (p, get_transput_buffer (CONTENT_BUFFER), get_transput_buffer_index (CONTENT_BUFFER));
  if (k != 0) {
// Not gracefully closed by recv ().
    ASSERT (close (socket_id) == 0);
  }
  PUSH_VALUE (p, errno, A68_INT);
  WSACleanup ();
}

#endif

#endif
