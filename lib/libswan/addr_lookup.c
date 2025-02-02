/*
 * addr_lookup: resolve_defaultroute_one() -- attempt to resolve a default route
 *
 * Copyright (C) 2005 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2012-2014 Paul Wouters <paul@libreswan.org>
 * Copyright (C) 2014 D. Hugh Redelmeier <hugh@mimosa.com>
 * Copyright (C) 2012-2013 Kim B. Heino <b@bbbs.net>
 * Copyright (C) 2021 Andrew Cagney
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/version.h>	/* RTA_UID hack */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <net/if.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "constants.h"
#include "lswalloc.h"
#include "ipsecconf/confread.h"
#include "kernel_netlink_reply.h"
#include "addr_lookup.h"
#ifdef USE_DNSSEC
# include "dnssec.h"
#endif

#include "ip_info.h"
#include "lswlog.h"

#define verbose(FMT, ...) if (verbose_rc_flags) llog(verbose_rc_flags, logger, FMT, ##__VA_ARGS__)

static void resolve_point_to_point_peer(const char *interface,
					const struct ip_info *afi,
					ip_address *peer,
					lset_t verbose_rc_flags,
					struct logger *logger)
{
	struct ifaddrs *ifap;

	/* Get info about all interfaces */
	if (getifaddrs(&ifap) != 0)
		return;

	/* Find the right interface, if any */
	for (const struct ifaddrs *ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_POINTOPOINT) == 0) {
			continue;
		}

		if (!streq(ifa->ifa_name, interface)) {
			continue;
		}

		struct sockaddr *sa = ifa->ifa_ifu.ifu_dstaddr;
		if (sa == NULL || sa->sa_family != afi->af) {
			continue;
		}

		ip_sockaddr isa = {
			.len = sizeof(*sa),
			.sa.sa = *sa,
		};
		err_t err = sockaddr_to_address_port(isa, peer,
						     NULL/*ignore port*/);
		if (err != NULL) {
			verbose("  interface %s had invalid sockaddr: %s",
				interface, err);
			continue;
		}

		address_buf ab;
		verbose("  found peer %s to interface %s",
			str_address(peer, &ab), interface);
		break;
	}
	freeifaddrs(ifap);
}

/*
 * Buffer size for netlink query (~100 bytes) and replies.
 * More memory will be allocated dynamically when needed for replies.
 * If DST is specified, reply will be ~100 bytes.
 * If DST is not specified, full route table will be returned.
 * On 64bit systems 100 route entries requires about 6KiB.
 *
 * When reading data from netlink the final packet in each recvfrom()
 * will be truncated if it doesn't fit to buffer. Netlink returns up
 * to 16KiB of data so always keep that much free.
 */
#define RTNL_BUFSIZE (NL_BUFMARGIN + 8192)

/*
 * Initialize netlink query message.
 */

static struct nlmsghdr *netlink_query_init(const struct ip_info *afi,
					   char type, int flags,
					   lset_t verbose_rc_flags,
					   struct logger *logger)
{
	struct nlmsghdr *nlmsg = alloc_bytes(RTNL_BUFSIZE, "netlink query");
	struct rtmsg *rtmsg;

	nlmsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsg->nlmsg_flags = flags;
	nlmsg->nlmsg_type = type;
	nlmsg->nlmsg_seq = 0;
	nlmsg->nlmsg_pid = getpid();

	rtmsg = (struct rtmsg *)NLMSG_DATA(nlmsg);
	rtmsg->rtm_family = afi->af;
	rtmsg->rtm_table = 0; /* RT_TABLE_MAIN doesn't seem to do much */
	rtmsg->rtm_protocol = 0;
	rtmsg->rtm_scope = 0;
	rtmsg->rtm_type = 0;
	rtmsg->rtm_src_len = 0;
	rtmsg->rtm_dst_len = 0;
	rtmsg->rtm_tos = 0;

	verbose("  query %s%s%s%s",
		(type == RTM_GETROUTE ? "getroute" : "?"),
		(flags & NLM_F_REQUEST ? " +REQUEST" : ""),
		/*NLM_F_DUMP==NLM_F_ROOT|NLM_F_MATCH*/
		(flags & NLM_F_ROOT ? " +ROOT" : ""),
		(flags & NLM_F_MATCH ? " +MATCH" : ""));

	return nlmsg;
}

/*
 * Add RTA_SRC or RTA_DST attribute to netlink query message.
 */
static void netlink_query_add(struct nlmsghdr *nlmsg, int rta_type,
			      const ip_address *addr, const char *what,
			      lset_t verbose_rc_flags, struct logger *logger)
{
	struct rtmsg *rtmsg;
	struct rtattr *rtattr;
	int rtlen;

	rtmsg = (struct rtmsg *)NLMSG_DATA(nlmsg);

	/* Find first empty attribute slot */
	rtlen = RTM_PAYLOAD(nlmsg);
	rtattr = (struct rtattr *)RTM_RTA(rtmsg);
	while (RTA_OK(rtattr, rtlen))
		rtattr = RTA_NEXT(rtattr, rtlen);

	/* Add attribute */
	shunk_t bytes = address_as_shunk(addr);
	rtattr->rta_type = rta_type;
	rtattr->rta_len = sizeof(struct rtattr) + bytes.len; /* bytes */
	memmove(RTA_DATA(rtattr), bytes.ptr, bytes.len);
	if (rta_type == RTA_SRC)
		rtmsg->rtm_src_len = bytes.len * 8; /* bits */
	else
		rtmsg->rtm_dst_len = bytes.len * 8;
	nlmsg->nlmsg_len += rtattr->rta_len;

	address_buf ab;
	verbose("  add %s %s (%s)",
		(rta_type == RTA_DST ? "dst" :
		 rta_type == RTA_GATEWAY ? "gateway" :
		 rta_type == RTA_SRC ? "src" :
		 rta_type == RTA_PREFSRC ? "prefsrc" :
		 "???"),
		str_address(addr, &ab), what);
}

/*
 * Send netlink query message and read reply.
 */
static ssize_t netlink_query(struct nlmsghdr **nlmsgp, size_t bufsize)
{
	int sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

	if (sock < 0) {
		int e = errno;

		printf("create netlink socket failure: (%d: %s)\n", e, strerror(e));
		return -1;
	}

	/* Send request */
	struct nlmsghdr *nlmsg = *nlmsgp;

	if (send(sock, nlmsg, nlmsg->nlmsg_len, 0) < 0) {
		int e = errno;

		printf("write netlink socket failure: (%d: %s)\n", e, strerror(e));
		close(sock);
		return -1;
	}

	/*
	 * Read response; netlink_read_reply() may re-allocate buffer.
	 */
	errno = 0;	/* in case failure does not set it */
	ssize_t len = netlink_read_reply(sock, (char**)nlmsgp, bufsize, 1, getpid());

	if (len < 0)
		printf("read netlink socket failure: (%d: %s)\n",
			errno, strerror(errno));

	close(sock);
	return len;
}

/*
 * See if left->addr or left->next is %defaultroute and change it to IP.
 */

static const char *pa(enum keyword_host type, ip_address address, const char *hostname, address_buf *buf)
{
	switch (type) {
	case KH_NOTSET: return "<not-set>";
	case KH_DEFAULTROUTE: return "<defaultroute>";
	case KH_ANY: return "<any>";
	case KH_IFACE: return hostname;
	case KH_OPPO: return "<oppo>";
	case KH_OPPOGROUP: return "<oppogroup>";
	case KH_GROUP: return "<group>";
	case KH_IPHOSTNAME: return hostname;
	case KH_IPADDR: return str_address(&address, buf);
	default: return "<other>";
	}
}

enum resolve_status resolve_defaultroute_one(struct starter_end *host,
					     struct starter_end *peer,
					     lset_t verbose_rc_flags,
					     struct logger *logger)
{
	/*
	 * "left="         == host->addrtype and host->addr
	 * "leftnexthop="  == host->nexttype and host->nexthop
	 */
	const struct ip_info *afi = host->host_family;

	address_buf ab, gb, pb;
	verbose("resolving src = %s gateway = %s peer %s",
		pa(host->addrtype, host->addr, host->strings[KSCF_IP], &ab),
		pa(host->nexttype, host->nexthop, host->strings[KSCF_NEXTHOP], &gb),
		pa(peer->addrtype, peer->addr, peer->strings[KSCF_IP], &pb));


	/*
	 * Can only resolve one at a time.
	 *
	 * XXX: OLD comments:
	 *
	 * If we have for example host=%defaultroute + peer=%any
	 * (no destination) the netlink reply will be full routing table.
	 * We must do two queries:
	 * 1) find out default gateway
	 * 2) find out src for that default gateway
	 *
	 * If we have only peer IP and no gateway/src we must
	 * do two queries:
	 * 1) find out gateway for dst
	 * 2) find out src for that gateway
	 * Doing both in one query returns src for dst.
	 */
	enum seeking { NOTHING, PREFSRC, GATEWAY, } seeking
		= (host->nexttype == KH_DEFAULTROUTE ? GATEWAY :
		   host->addrtype == KH_DEFAULTROUTE ? PREFSRC :
		   NOTHING);
	verbose("  seeking %s",
		(seeking == NOTHING ? "nothing" :
		 seeking == PREFSRC ? "prefsrc" :
		 seeking == GATEWAY ? "gateway" :
		 "?"));
	if (seeking == NOTHING) {
		return RESOLVE_SUCCESS;	/* this end already figured out */
	}

	/*
	 * msgbuf is dynamically allocated since the buffer may need
	 * to be grown.
	 */
	struct nlmsghdr *msgbuf =
		netlink_query_init(afi, /*type*/RTM_GETROUTE,
				   (/*flags*/NLM_F_REQUEST |
				    (seeking == GATEWAY ? NLM_F_DUMP : 0)),
				   verbose_rc_flags, logger);

	/*
	 * If known, add a destination address.  Either the peer, or
	 * the gateway.
	 */

	const bool has_peer = (peer->addrtype == KH_IPADDR || peer->addrtype == KH_IPHOSTNAME);
	bool added_dst;
	if (host->nexttype == KH_IPADDR && afi == &ipv4_info) {
		pexpect(seeking == PREFSRC);
		/*
		 * My nexthop (gateway) is specified.
		 * We need to figure out our source IP to get there.
		 */

		/*
		 * AA_2019 Why use nexthop and not peer->addr to look up src address?
		 * The lore is that there is an (old) bug when looking up IPv4 src
		 * IPv6 with gateway link local address will return link local
		 * address and not the global address.
		 */
		added_dst = true;
		netlink_query_add(msgbuf, RTA_DST, &host->nexthop,
				  "host->nexthop", verbose_rc_flags, logger);
	} else if (has_peer) {
		/*
		 * Peer IP is specified.
		 * We may need to figure out source IP
		 * and gateway IP to get there.
		 *
		 * XXX: should this also update peer->addrtype?
		 */
		pexpect(peer->host_family != NULL);
		if (peer->addrtype == KH_IPHOSTNAME) {
#ifdef USE_DNSSEC
			/* try numeric first */
			err_t er = ttoaddress_num(shunk1(peer->strings[KSCF_IP]),
						  peer->host_family, &peer->addr);
			if (er != NULL) {
				/* not numeric, so resolve it */
				if (!unbound_resolve(peer->strings[KSCF_IP],
						     peer->host_family,
						     &peer->addr,
						     logger)) {
					pfree(msgbuf);
					return RESOLVE_FAILURE;
				}
			}
#else
			err_t er = ttoaddress_dns(shunk1(peer->strings[KSCF_IP]),
						  peer->host_family, &peer->addr);
			if (er != NULL) {
				pfree(msgbuf);
				return RESOLVE_FAILURE;
			}
#endif
		} else {
			pexpect(peer->addrtype == KH_IPADDR);
		}
		added_dst = true;
		netlink_query_add(msgbuf, RTA_DST, &peer->addr,
				  "peer->addr", verbose_rc_flags, logger);
	} else if (host->nexttype == KH_IPADDR &&
		   (peer->addrtype == KH_GROUP ||
		    peer->addrtype == KH_OPPOGROUP)) {
		added_dst = true;
		netlink_query_add(msgbuf, RTA_DST, &host->nexthop,
				  "host->nexthop peer=group",
				  verbose_rc_flags, logger);
	} else {
		added_dst = false;
	}

	if (added_dst && host->addrtype == KH_IPADDR) {
		/* SRC works only with DST */
		pexpect(seeking == GATEWAY);
		netlink_query_add(msgbuf, RTA_SRC, &host->addr,
				  "host->addr", verbose_rc_flags, logger);
	}

	/* Send netlink get_route request */
	ssize_t len = netlink_query(&msgbuf, RTNL_BUFSIZE);

	if (len < 0) {
		pfree(msgbuf);
		return RESOLVE_FAILURE;
	}

	/* Parse reply */
	struct nlmsghdr *nlmsg = (struct nlmsghdr *)msgbuf;

	enum resolve_status status = RESOLVE_FAILURE; /* assume the worst */
	for (; NLMSG_OK(nlmsg, (size_t)len) && status == RESOLVE_FAILURE;
	     nlmsg = NLMSG_NEXT(nlmsg, len)) {

		if (nlmsg->nlmsg_type == NLMSG_DONE)
			break;

		if (nlmsg->nlmsg_type == NLMSG_ERROR) {
			llog(RC_LOG, logger, "netlink error");
			pfree(msgbuf);
			return RESOLVE_FAILURE;
		}

		/* ignore all but IPv4 and IPv6 */
		struct rtmsg *rtmsg = (struct rtmsg *) NLMSG_DATA(nlmsg);
		if (rtmsg->rtm_family != afi->af) {
			continue;
		}

		/* Parse one route entry */

		char r_interface[IF_NAMESIZE+1];
		zero(&r_interface);
		ip_address src = unset_address;
		ip_address prefsrc = unset_address;
		ip_address gateway = unset_address;
		ip_address dst = unset_address;
		int priority = -1;
		signed char pref = -1;
		int table;
		const char *cacheinfo = "";
		const char *uid = "";

		struct rtattr *rtattr = (struct rtattr *) RTM_RTA(rtmsg);
		int rtlen = RTM_PAYLOAD(nlmsg);

		while (RTA_OK(rtattr, rtlen)) {
			const void *data = RTA_DATA(rtattr);
			unsigned len = RTA_PAYLOAD(rtattr);
			err_t err;
			switch (rtattr->rta_type) {
			case RTA_OIF:
				if_indextoname(*(int *)RTA_DATA(rtattr),
					       r_interface);
				break;
			case RTA_PREFSRC:
				err = data_to_address(data, len, afi, &prefsrc);
				if (err != NULL) {
					verbose("  unknown prefsrc from kernel: %s", err);
				}
				break;
			case RTA_GATEWAY:
				err = data_to_address(data, len, afi, &gateway);
				if (err != NULL) {
					verbose("  unknown gateway from kernel: %s", err);
				}
				break;
			case RTA_DST:
				err = data_to_address(data, len, afi, &dst);
				if (err != NULL) {
					verbose("  unknown dst from kernel: %s", err);
				}
				break;
			case RTA_SRC:
				err = data_to_address(data, len, afi, &src);
				if (err != NULL) {
					verbose("  unknown src from kernel: %s", err);
				}
				break;
			case RTA_PRIORITY:
				if (len != sizeof(priority)) {
					verbose("  ignoring PRIORITY with wrong size %d", len);
					break;
				}
				memcpy(&priority, data, len);
				break;
			case RTA_PREF:
				if (len != sizeof(pref)) {
					verbose("  ignoring PREF with wrong size %d", len);
					break;
				}
				memcpy(&pref, data, len);
				break;
			case RTA_TABLE:
				if (len != sizeof(table)) {
					verbose("  ignoring TABLE with wrong size %d", len);
					break;
				}
				memcpy(&table, data, len);
				break;
			case RTA_CACHEINFO:
				cacheinfo = " +cacheinfo";
				break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,11,16)
			case RTA_UID:
				/*
				 * XXX: Above kernel version matches
				 * when this code was added to this
				 * file and not when RTA_UID was added
				 * to the kernel herders.  That was:
				 *
				 *  commit 4fb74506838b3e34eaaebfcf90ebcd1fd52ab813
				 *  Merge: 0d53072aa42b e2d118a1cb5e
				 *  Author: David S. Miller <davem@davemloft.net>
				 *  Date:   Fri Nov 4 14:45:24 2016 -0400
				 *
				 *    Merge branch 'uid-routing'
				 *
				 * but who knows what that kernel
				 * version was.
				 *
				 * A sane kernel would include:
				 *
				 *   #define RTA_UID RTA_UID
				 *
				 * when adding the enum so that:
				 *
				 *   #ifdef RTA_ID
				 *
				 * could do the right thing.  Sigh.
				 */
				uid = " +uid";
				break;
#endif
			default:
				verbose("  ignoring %d", rtattr->rta_type);
			}
			rtattr = RTA_NEXT(rtattr, rtlen);
		}

		/*
		 * Ignore if not main table.
		 * Ignore ipsecX or mastX interfaces.
		 *
		 * XXX: instead of rtm_table, should this be checking
		 * TABLE?
		 */
		const char *ignore = ((rtmsg->rtm_table != RT_TABLE_MAIN) ? "; ignore: wrong table" :
				      startswith(r_interface, "ipsec") ? "; ignore: ipsec interface" :
				      startswith(r_interface, "mast") ? "; ignore: mast interface" :
				      NULL);

		address_buf sb, psb, db, gb;
		verbose("  src %s prefsrc %s gateway %s dst %s dev %s priority %d pref %d table %d%s%s%s",
			str_address(&src, &sb),
			str_address(&prefsrc, &psb),
			str_address(&gateway, &gb),
			str_address(&dst, &db),
			(r_interface[0] ? r_interface : "?"),
			priority, pref, rtmsg->rtm_table,
			cacheinfo, uid,
			(ignore != NULL ? ignore : ""));

		if (ignore != NULL)
			continue;

		switch (seeking) {
		case PREFSRC:
			if (!address_is_unset(&prefsrc)) {
				status = RESOLVE_SUCCESS;
				host->addrtype = KH_IPADDR;
				host->addr = prefsrc;
				address_buf ab;
				verbose("  found prefsrc (host_addr): %s",
					str_address(&host->addr, &ab));
			}
			break;
		case GATEWAY:
			if (address_is_unset(&dst)) {
				if (address_is_unset(&gateway) && r_interface[0] != '\0') {
					/*
					 * Point-to-Point default gw without
					 * "via IP".  Attempt to find gateway
					 * as the IP address on the interface.
					 */
					resolve_point_to_point_peer(r_interface, host->host_family,
								    &gateway, verbose_rc_flags,
								    logger);
				}
				if (!address_is_unset(&gateway)) {
					/*
					 * Note: Use first even if
					 * multiple.
					 *
					 * XXX: assume a gateway
					 * always requires a second
					 * call to get PREFSRC, code
					 * above will quickly return
					 * when it isn't.
					 */
					status = RESOLVE_PLEASE_CALL_AGAIN;
					host->nexttype = KH_IPADDR;
					host->nexthop = gateway;
					address_buf ab;
					verbose("  found gateway (host_nexthop): %s",
						str_address(&host->nexthop, &ab));
				}
			}
			break;
		default:
			bad_case(seeking);
		}
	}
	pfree(msgbuf);

	verbose("  %s: src = %s gateway = %s",
		(status == RESOLVE_FAILURE ? "failure" :
		 status == RESOLVE_SUCCESS ? "success" :
		 status == RESOLVE_PLEASE_CALL_AGAIN ? "please-call-again" :
		 "???"),
		pa(host->addrtype, host->addr, host->strings[KSCF_IP], &ab),
		pa(host->nexttype, host->nexthop, host->strings[KSCF_NEXTHOP], &gb));
	return status;
}
