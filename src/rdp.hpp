#ifndef RDP_HPP
#define RDP_HPP

#include "cagetypes.hpp"
#include "common.hpp"

#include <stdint.h>
#include <time.h>

#include <vector>
#include <functional>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp> 
#include <boost/function.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#define RDP_VER 2

namespace libcage {
        enum rdp_state {
                CLOSED,
                LISTEN,
                SYN_SENT,
                SYN_RCVD,
                OPEN,
                CLOSE_WAIT,
        };

        struct rdp_head {
                uint8_t  flags;
                uint8_t  hlen;
                uint16_t sport;
                uint16_t dport;
                uint16_t dlen;
                uint32_t seqnum;
                uint32_t acknum;
                uint8_t  checksum;
        };

        struct rdp_syn {
                rdp_head head;
                uint16_t out_segs_max;
                uint16_t seg_size_max;
                uint16_t options;
        };


        class rdp_con;
        typedef boost::shared_ptr<rdp_con> rdp_con_ptr;

        class rdp_addr {
        public:
                id_ptr          did;   // destination id
                uint16_t        dport; // destination port
                uint16_t        sport; // source port

                bool operator== (const rdp_addr &addr) const
                {
                        return *did == *addr.did && dport == addr.dport &&
                                sport == addr.sport;
                }
        };

        size_t hash_value(const rdp_addr &addr);


        class rdp {
        public:
                static const uint8_t   flag_syn;
                static const uint8_t   flag_ack;
                static const uint8_t   flag_eak;
                static const uint8_t   flag_rst;
                static const uint8_t   flag_nul;
                static const uint8_t   flag_ver;

                static const uint32_t  rbuf_max_default;
                static const uint32_t  snd_max_default;
                static const uint16_t  well_known_port_max;

                rdp();
                virtual ~rdp();

                int             listen(uint16_t sport); // passive open
                int             connect(uint16_t sport, id_ptr did,
                                        uint16_t dport); // active open
                int             accept(uint16_t con);
                void            close(int con);
                int             send(int con, const void *buf, int len);
                void            receive(int con, void *buf, int *len);
                rdp_state       status(int con);

                // input and output datagram to under layer
                typedef boost::function<void (id_ptr dst, const void *buf,
                                              int len)> callback_output;
                void            input_dgram(id_ptr src, const void *buf,
                                            int len);
                void            in_state_closed(rdp_addr addr, rdp_head *head,
                                                int len);
                void            in_state_listen(rdp_addr addr, rdp_head *head,
                                                int len);
                void            in_state_closed_wait(rdp_con_ptr con,
                                                     rdp_addr addr,
                                                     rdp_head *head, int len);
                void            in_state_syn_sent(rdp_con_ptr con,
                                                  rdp_addr addr,
                                                  rdp_head *head, int len);
                void            in_state_syn_rcvd(rdp_con_ptr con,
                                                  rdp_addr addr,
                                                  rdp_head *head, int len);
                void            in_state_open(rdp_con_ptr con,
                                                  rdp_addr addr,
                                                  rdp_head *head, int len);
                void            set_callback_output(callback_output func);

        private:
                typedef boost::bimaps::unordered_set_of<uint16_t> _uint16_set;
                typedef boost::bimaps::unordered_set_of<int>      _int_set;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>            listening_t;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>::value_type listening_val;

                boost::unordered_set<int>       m_desc_set;
                listening_t                     m_listening;
                
                boost::unordered_map<rdp_addr, rdp_con_ptr>     m_addr2conn;
                boost::unordered_map<int, rdp_con_ptr>          m_desc2conn;

                callback_output         m_output_func;

                void            set_syn_option_seq(uint16_t &options,
                                                   bool sequenced);
        };

        class rdp_con {
        public:
                rdp_addr        addr;

                rdp_state       state;     // The current state of the
                                           // connection.
                time_t          closewait; // A timer used to time out the
                                           // CLOSE-WAIT state.
                uint32_t        sbuf_max;  // The largest possible segment (in
                                           // octets) that can legally be sent.
                                           // This variable is specified by the
                                           // foreign host in the SYN segment
                                           // during connection establishment.
                uint32_t        rbuf_max;  // The largest possible segment (in
                                           // octets) that can be received. This
                                           // variable is specified by the user
                                           // when the connection is opened. The
                                           // variable is sent to the foreign
                                           // host in the SYN segment.

                // Send Sequence Number Variables:
                uint32_t        snd_nxt; // The sequence number of the next
                                         // segment that is to be sent.
                uint32_t        snd_una; // The sequence number of the oldest
                                         // unacknowledged segment.
                uint32_t        snd_max; // The maximum number of outstanding
                                         // (unacknowledged) segments that can
                                         // be sent. The sender should not send
                                         // more than this number of segments
                                         // without getting an acknowledgement.
                uint32_t        snd_iss; // The initial send sequence number.
                                         // This is the sequence number that was
                                         // sent in the SYN segment.

                // Receive Sequence Number Variables:
                uint32_t        rcv_cur; // The sequence number of the last
                                         // segment received correctly and in
                                         // sequence.
                uint32_t        rcv_max; // The maximum number of segments that
                                         // can be buffered for this connection.
                std::vector<uint32_t>   rcvdsendq; // The array of sequence
                                                   // numbers of segments that
                                                   // have been received and
                                                   // acknowledged out of
                                                   // sequence.

                // Variables from Current Segment:
                uint32_t        seg_seq;  // The sequence number of the segment
                                          // currently being processed.
                uint32_t        seg_ack;  // The acknowledgement sequence
                                          // number in the segment currently
                                          // being processed.
                uint32_t        seg_max;  // The maximum number of outstanding
                                          // segments the receiver is willing to
                                          // hold, as specified in the SYN
                                          // segment that established the
                                          // connection.
                uint32_t        seg_bmax; // The maximum segment size (in
                                          // octets) accepted by the foreign
                                          // host on a connection, as specified
                                          // in the SYN segment that established
                                          // the connection.
        };
}

#endif // RDP_HPP