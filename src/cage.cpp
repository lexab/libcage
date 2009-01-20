/*
 * Copyright (c) 2009, Yuuki Takano (ytakanoster@gmail.com).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the writers nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#include "cage.hpp"

#include <openssl/rand.h>

#include <boost/foreach.hpp>

#include "cagetypes.hpp"

namespace libcage {
        void
        cage::udp_receiver::operator() (udphandler &udp, void *buf, int len,
                                        sockaddr *from, int fromlen,
                                        bool is_timeout)
        {
                if (len < (int)sizeof(msg_hdr)) {
                        return;
                }

                msg_hdr *hdr = (msg_hdr*)buf;

                if (ntohs(hdr->magic) != MAGIC_NUMBER ||
                    hdr->ver != CAGE_VERSION) {
                        return;
                }

                switch (hdr->type) {
                case type_nat_echo:
                        if (len == (int)sizeof(msg_nat_echo)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo(buf, from, fromlen);
                        }
                        break;
                case type_nat_echo_reply:
                        if (len == (int)sizeof(msg_nat_echo_reply)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo_reply\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo_reply(buf, from,
                                                             fromlen);
                        }
                        break;
                case type_nat_echo_redirect:
                        if (len == (int)sizeof(msg_nat_echo_redirect)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo_redirect\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo_redirect(buf, from,
                                                                fromlen);
                        }
                        break;
                case type_dtun_ping:
                        if (len == (int)sizeof(msg_dtun_ping)) {
                                m_cage.m_dtun.recv_ping(buf, from, fromlen);
                        }
                        break;
                case type_dtun_ping_reply:
                        if (len == (int)sizeof(msg_dtun_ping_reply)) {
                                m_cage.m_dtun.recv_ping_reply(buf, from,
                                                              fromlen);
                        }
                        break;
                case type_dtun_find_node:
                        if (len == (int)sizeof(msg_dtun_find_node)) {
                                m_cage.m_dtun.recv_find_node(buf, from,
                                                             fromlen);
                        }
                        break;
                case type_dtun_find_node_reply:
                        if (len >= (int)(sizeof(msg_dtun_find_node_reply) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dtun.recv_find_node_reply(buf, len,
                                                                   from);
                        }
                        break;
                case type_dtun_register:
                        if (len == (int)sizeof(msg_dtun_register)) {
                                m_cage.m_dtun.recv_register(buf, from);
                        }
                        break;
                case type_dtun_find_value:
                        if (len == (int)sizeof(msg_dtun_find_value)) {
                                m_cage.m_dtun.recv_find_value(buf, from,
                                                              fromlen);
                        }
                        break;
                case type_dtun_find_value_reply:
                        if (len >= (int)(sizeof(msg_dtun_find_value_reply) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dtun.recv_find_value_reply(buf, len,
                                                                    from);
                        }
                        break;
                }
        }

        cage::cage() : m_nat(m_udp, m_timer, m_id),
                       m_receiver(*this),
                       m_peers(m_timer),
                       m_dtun(m_id, m_timer, m_peers, m_nat, m_udp)
        {
                unsigned char buf[20];

                RAND_pseudo_bytes(buf, sizeof(buf));
                m_id.from_binary(buf, sizeof(buf));
        }

        bool
        cage::open(int domain, uint16_t port)
        {
                if (!m_udp.open(domain, port))
                        return false;

                m_udp.set_callback(&m_receiver);

                return true;
        }

#ifdef DEBUG_NAT
        void
        cage::test_natdetect()
        {
                cage *c1, *c2;
                c1 = new cage();
                c2 = new cage();

                c1->open(PF_INET, 0);
                c2->open(PF_INET, 0);

                c1->m_nat.detect_nat("localhost", ntohs(c2->m_udp.get_port()));
        }

        void
        cage::test_nattypedetect()
        {
                cage *c1, *c2, *c3;
                c1 = new cage();
                c2 = new cage();
                c3 = new cage();

                c1->open(PF_INET, 0);
                c2->open(PF_INET, 0);
                c3->open(PF_INET, 0);

                c1->m_nat.set_state_nat();

                c1->m_nat.detect_nat_type("localhost",
                                          ntohs(c2->m_udp.get_port()),
                                          "localhost",
                                          ntohs(c3->m_udp.get_port()));

        }
#endif // DEBUG_NAT

#ifdef DEBUG
        #define NUM_NODES 100

        void
        cage::dtun_find_node_callback::operator() (std::vector<cageaddr> &addrs)
        {
                printf("recv find node reply\n");

                BOOST_FOREACH(cageaddr &addr, addrs) {
                        in_ptr in = boost::get<in_ptr>(addr.saddr);
                        printf("  port = %d, ", ntohs(in->sin_port));
                        printf("id = %s\n", addr.id->to_string().c_str());
                }

                p_cage[n].m_dtun.register_node();
                p_cage[n].m_dtun.print_table();

                n++;

                if (n < NUM_NODES) {
                        p_cage[n].open(PF_INET, 11000 + n);
                        p_cage[n].m_nat.set_state_global();
                        p_cage[n].m_dtun.find_node("localhost", 10000,
                                                    *this);
                } else {
                        dtun_find_value_callback func;

                        p_cage[0].m_dtun.find_value(p_cage[NUM_NODES - 2].m_id,
                                                    func);
                }
        }

        void
        cage::dtun_find_value_callback::operator() (bool result, cageaddr &addr)
        {
                printf("recv find value reply\n");

                if (result)
                        printf("  true\n");
                else
                        printf("  false\n");
        }

        void
        cage::test_dtun()
        {
                dtun_find_node_callback func;
                cage *c;

                // open bootstrap node
                c = new cage;
                c->open(PF_INET, 10000);
                c->m_nat.set_state_global();

                // connect to bootstrap
                func.n = 0;
                func.p_cage = new cage[NUM_NODES];

                func.p_cage[0].open(PF_INET, 11000);
                func.p_cage[0].m_nat.set_state_global();
                func.p_cage[0].m_dtun.find_node("localhost", 10000, func);
        }
#endif // DEBUG
}