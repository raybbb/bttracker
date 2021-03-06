/*
 * Copyright (c) 2013, BtTracker Authors
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

int
bt_ipv4_udp_sock(const char *addr, uint16_t port, struct addrinfo **addrinfo)
{
  struct addrinfo hints = {
    .ai_family = AF_INET,      // IPv4
    .ai_socktype = SOCK_DGRAM,
    .ai_flags = AI_PASSIVE
  };

  char portstr[10];
  sprintf(portstr, "%d", port);

  if (getaddrinfo(addr, portstr, &hints, addrinfo) != 0) {
    syslog(LOG_ERR, "Error in getaddrinfo(). Exiting");
    exit(BT_EXIT_NETWORK_ERROR);
  }

  int sock;
  sock = socket((*addrinfo)->ai_family, (*addrinfo)->ai_socktype,
                       (*addrinfo)->ai_protocol);
  if (-1 == sock) {
    syslog(LOG_ERR, "Cannot create socket. Exiting");
    exit(BT_EXIT_NETWORK_ERROR);
  }

  return sock;
}

void
bt_read_request_data(const char *buffer, bt_req_t *req)
{
  req->connection_id  = ntohll(*((int64_t *) buffer));
  req->action         = ntohl (*((int32_t *) (buffer + 8)));
  req->transaction_id = ntohl (*((int32_t *) (buffer + 12)));
}

void
bt_write_error_data(char *resp_buffer, bt_req_t *req, const char* msg)
{
  /* Converts the response data to network byte order. */
  req->connection_id  = htonll(req->connection_id);
  req->action         = htonl (req->action);
  req->transaction_id = htonl (req->transaction_id);

  /* Writes each field of the error response. */
  memcpy(resp_buffer,     &req->action, 4);
  memcpy(resp_buffer + 4, &req->transaction_id, 4);
  memcpy(resp_buffer + 8, msg, strlen(msg) + 1);
}

void
bt_write_connection_data(char *resp_buffer, bt_connection_resp_t *resp)
{
  /* Converts the response data to network byte order. */
  resp->action         = htonl (resp->action);
  resp->transaction_id = htonl (resp->transaction_id);
  resp->connection_id  = htonll(resp->connection_id);

  /* Writes each field of the connection response. */
  memcpy(resp_buffer,     &resp->action, 4);
  memcpy(resp_buffer + 4, &resp->transaction_id, 4);
  memcpy(resp_buffer + 8, &resp->connection_id, 8);
}

void
bt_read_announce_request_data(const char *buffer, bt_announce_req_t *req)
{
  /* Array of bytes do not need conversion. */
  memcpy(&req->info_hash, buffer + 16, 20);
  memcpy(&req->peer_id,   buffer + 36, 20);

  /* Converts data to host byte order. */
  req->connection_id  = ntohll(*((int64_t *)  buffer));
  req->action         = ntohl (*((int32_t *)  (buffer + 8)));
  req->transaction_id = ntohl (*((int32_t *)  (buffer + 12)));
  req->downloaded     = ntohll(*((int64_t *)  (buffer + 56)));
  req->left           = ntohll(*((int64_t *)  (buffer + 64)));
  req->uploaded       = ntohll(*((int64_t *)  (buffer + 72)));
  req->event          = ntohl (*((int32_t *)  (buffer + 80)));
  req->ipv4_addr      = ntohl (*((uint32_t *) (buffer + 84)));
  req->key            = ntohl (*((int32_t *)  (buffer + 88)));
  req->num_want       = ntohl (*((int32_t *)  (buffer + 92)));
  req->port           = ntohs (*((uint16_t *) (buffer + 96)));
}

void
bt_write_announce_response_data(char *resp_buffer, bt_announce_resp_t *resp)
{
  /* Converts data to network byte order. */
  resp->action         = htonl(resp->action);
  resp->transaction_id = htonl(resp->transaction_id);
  resp->interval       = htonl(resp->interval);
  resp->leechers       = htonl(resp->leechers);
  resp->seeders        = htonl(resp->seeders);

  /* Writes each field of the announce response. */
  memcpy(resp_buffer,      &resp->action, 4);
  memcpy(resp_buffer +  4, &resp->transaction_id, 4);
  memcpy(resp_buffer +  8, &resp->interval, 4);
  memcpy(resp_buffer + 12, &resp->leechers, 4);
  memcpy(resp_buffer + 16, &resp->seeders, 4);
}

void
bt_write_announce_peer_data(char *resp_buffer, bt_list *peers)
{
  bt_list *current_peer = peers;
  int npeer = 0;

  while (current_peer != NULL) {
    bt_peer_addr_t *peer_addr = current_peer->data;

    /* Converts data to network byte order. */
    peer_addr->ipv4_addr = htonl(peer_addr->ipv4_addr);
    peer_addr->port      = htons(peer_addr->port);

    /* Writes the address for each peer in the response buffer. */
    memcpy(resp_buffer + 20 + 6 * npeer, &peer_addr->ipv4_addr, 4);
    memcpy(resp_buffer + 24 + 6 * npeer++, &peer_addr->port, 2);

    current_peer = bt_list_next(current_peer);
  }
}

void
bt_read_scrape_request_data(const char *buffer, size_t buflen, bt_scrape_req_t *req)
{
  req->info_hash_len = (buflen - 16) / 20;

  /* Array of info_hashes, do not need conversion. */
  memcpy(req->info_hash, buffer+16, buflen - 16);

  /* Converts data to host byte order. */
  req->connection_id  = ntohll(*((int64_t *) buffer));
  req->action         = ntohl (*((int32_t *) (buffer + 8)));
  req->transaction_id = ntohl (*((int32_t *) (buffer + 12)));
}

void
bt_write_scrape_response_data(char *resp_buffer, bt_scrape_resp_t *resp)
{
  bt_list *current_entry = resp->scrape_entries;
  int nentry = 0;

  /* Converts data to network byte order. */
  resp->action         = htonl(resp->action);
  resp->transaction_id = htonl(resp->transaction_id);

  /* Writes each field of the announce response. */
  memcpy(resp_buffer,     &resp->action, 4);
  memcpy(resp_buffer + 4, &resp->transaction_id, 4);

  while (current_entry != NULL) {
    bt_torrent_stats_t *entry = (bt_torrent_stats_t *) current_entry->data;

    /* Converts data to network byte order. */
    entry->seeders   = htonl(entry->seeders);
    entry->leechers  = htonl(entry->leechers);
    entry->downloads = htonl(entry->downloads);

    /* Writes the stats in the response buffer. */
    memcpy(resp_buffer + 8 + 12 * nentry++, entry, sizeof(bt_torrent_stats_t));

    current_entry = bt_list_next(current_entry);
  }
}
