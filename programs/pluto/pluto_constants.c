/* tables of names for values defined in constants.h
 *
 * Copyright (C) 1998-2002,2013 D. Hugh Redelmeier <hugh@mimosa.com>
 * Copyright (C) 2013-2019 Paul Wouters <pwouters@redhat.com>
 * Copyright (C) 2015-2019 Andrew Cagney <cagney@gnu.org>
 * Copyright (C) 2020 Yulia Kuzovkova <ukuzovkova@gmail.com>
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
 *
 */

/*
 * Note that the array sizes are all specified; this is to enable range
 * checking by code that only includes constants.h.
 */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#ifdef XFRM_SUPPORT
#include "linux/xfrm.h" /* local (if configured) or system copy */
#endif
#include "passert.h"

#include "jambuf.h"
#include "constants.h"
#include "enum_names.h"
#include "defs.h"
#include "kernel.h"

/*
 * To obsolete or convert to runtime options:
 * IPSEC_CONNECTION_LIMIT
 * NOTYET
 * NOT_YET
 * PFKEY
 * PLUTO_GROUP_CTL
 * SOFTREMOTE_CLIENT_WORKAROUND
 * USE_3DES USE_AES USE_MD5 USE_SHA1 USE_SHA2
 */

static const char *const kern_interface_name[] = {
	[USE_XFRM] = "netkey",
	[USE_BSDKAME] = "bsdkame",
};

enum_names kern_interface_names = {
	USE_XFRM, USE_BSDKAME,
	ARRAY_REF(kern_interface_name),
	"USE_", /* prefix */
	NULL
};

/* DPD actions */
static const char *const dpd_action_name[] = {
	"action:disabled",
	"action:clear",
	"action:hold",
	"action:restart",
};

enum_names dpd_action_names = {
	DPD_ACTION_DISABLED, DPD_ACTION_RESTART,
	ARRAY_REF(dpd_action_name),
	NULL, /* prefix */
	NULL
};

#ifdef XFRM_SUPPORT
/* netkey SA direction names */
static const char *const netkey_sa_dir_name[] = {
	"XFRM_IN",
	"XFRM_OUT",
	"XFRM_FWD",
};

enum_names netkey_sa_dir_names = {
	XFRM_POLICY_IN, XFRM_POLICY_FWD,
	ARRAY_REF(netkey_sa_dir_name),
	NULL, /* prefix */
	NULL
};
#endif

/* systemd watchdog action names */
static const char *const sd_action_name[] = {
	"action: exit", /* daemon exiting */
	"action: start", /* daemon starting */
	"action: watchdog", /* the keepalive watchdog ping */
	"action: reloading", /* the keepalive watchdog ping */
	"action: ready", /* the keepalive watchdog ping */
	"action: stopping", /* the keepalive watchdog ping */
};
enum_names sd_action_names = {
	PLUTO_SD_EXIT, PLUTO_SD_STOPPING,
	ARRAY_REF(sd_action_name),
	NULL, /* prefix */
	NULL
};

/* NAT methods */
static const char *const natt_method_name[] = {
	"none",
	"draft-ietf-ipsec-nat-t-ike-02/03",
	"draft-ietf-ipsec-nat-t-ike-05",
	"RFC 3947 (NAT-Traversal)",

	"I am behind NAT",
	"peer behind NAT",
};

enum_names natt_method_names = {
	NAT_TRAVERSAL_METHOD_none, NATED_PEER,
	ARRAY_REF(natt_method_name),
	NULL, /* prefix */
	NULL
};

/* routing status names */
static const char *const routing_story_strings[] = {
	"unrouted",             /* RT_UNROUTED: unrouted */
	"unrouted HOLD",        /* RT_UNROUTED_HOLD: unrouted, but HOLD shunt installed */
	"eroute eclipsed",      /* RT_ROUTED_ECLIPSED: RT_ROUTED_PROSPECTIVE except bare HOLD or instance has eroute */
	"prospective erouted",  /* RT_ROUTED_PROSPECTIVE: routed, and prospective shunt installed */
	"erouted HOLD",         /* RT_ROUTED_HOLD: routed, and HOLD shunt installed */
	"fail erouted",         /* RT_ROUTED_FAILURE: routed, and failure-context shunt eroute installed */
	"erouted",              /* RT_ROUTED_TUNNEL: routed, and erouted to an IPSEC SA group */
	"keyed, unrouted",      /* RT_UNROUTED_KEYED: was routed+keyed, but it got turned into an outer policy */
};

enum_names routing_story = {
	RT_UNROUTED, RT_UNROUTED_KEYED,
	ARRAY_REF(routing_story_strings),
	NULL, /* prefix */
	NULL };

static const char *const stf_status_strings[] = {
#define A(S) [S] = #S
	A(STF_SKIP_COMPLETE_STATE_TRANSITION),
	A(STF_IGNORE),
	A(STF_SUSPEND),
	A(STF_OK),
	A(STF_INTERNAL_ERROR),
	A(STF_V2_DELETE_IKE_AUTH_INITIATOR),
	A(STF_FATAL),
	A(STF_FAIL),
#undef A
};

enum_names stf_status_names = {
	0, elemsof(stf_status_strings)-1,
	ARRAY_REF(stf_status_strings),
	NULL, /* prefix */
	NULL
};

/*
 * Names for sa_policy_bits.
 */
static const char *const sa_policy_bit_name[] = {
#define P(N) [N##_IX] = #N
	P(POLICY_PSK),
	P(POLICY_RSASIG),
	P(POLICY_ECDSA),
	P(POLICY_AUTH_NEVER),
	P(POLICY_AUTH_NULL),
	P(POLICY_ENCRYPT),
	P(POLICY_AUTHENTICATE),
	P(POLICY_COMPRESS),
	P(POLICY_TUNNEL),
	P(POLICY_PFS),
	P(POLICY_DECAP_DSCP),
	P(POLICY_NOPMTUDISC),
	P(POLICY_MSDH_DOWNGRADE),
	P(POLICY_ALLOW_NO_SAN),
	P(POLICY_DNS_MATCH_ID),
	P(POLICY_SHA2_TRUNCBUG),
	P(POLICY_DONT_REKEY),
	P(POLICY_REAUTH),
	P(POLICY_OPPORTUNISTIC),
	P(POLICY_GROUP),
	P(POLICY_GROUTED),
	P(POLICY_GROUPINSTANCE),
	P(POLICY_UP),
	P(POLICY_XAUTH),
	P(POLICY_MODECFG_PULL),
	P(POLICY_AGGRESSIVE),
	P(POLICY_OVERLAPIP),
	P(POLICY_IKEV2_ALLOW_NARROWING),
	P(POLICY_IKEV2_PAM_AUTHORIZE),
	P(POLICY_SEND_REDIRECT_ALWAYS),
	P(POLICY_SEND_REDIRECT_NEVER),
	P(POLICY_ACCEPT_REDIRECT_YES),
	P(POLICY_IKE_FRAG_ALLOW),
	P(POLICY_IKE_FRAG_FORCE),
	P(POLICY_NO_IKEPAD),
	P(POLICY_MOBIKE),
	P(POLICY_PPK_ALLOW),
	P(POLICY_PPK_INSIST),
	P(POLICY_ESN_NO),
	P(POLICY_ESN_YES),
	P(POLICY_INTERMEDIATE),
	P(POLICY_IGNORE_PEER_DNS),
	P(POLICY_RSASIG_v1_5),
#undef P
};

enum_names sa_policy_bit_names = {
	0, POLICY_IX_LAST,
	ARRAY_REF(sa_policy_bit_name),
	"POLICY_", /* prefix */
	NULL
};


/*
 * Names for RFC 7427 IKEv2 AUTH signature hash algo sighash_policy_bits
 */
static const char *const sighash_policy_bit_name[] = {
	"SHA2_256",
	"SHA2_384",
	"SHA2_512",
};

const struct enum_names sighash_policy_bit_names = {
	POL_SIGHASH_SHA2_256_IX,
	POL_SIGHASH_SHA2_512_IX,
	ARRAY_REF(sighash_policy_bit_name),
	NULL, /* prefix */
	NULL, /* next */
};

static const char *const keyword_authby_name[] = {
	"unset",
	"never",
	"secret",
	"rsasig",
	"ecdsa",
	"null",
	"eaponly",
};

enum_names keyword_authby_names = {
	AUTHBY_UNSET, AUTHBY_EAPONLY,
	ARRAY_REF(keyword_authby_name),
	NULL, /* prefix */
	NULL
};

static const char *const allow_global_redirect_name[] = {
	"no",
	"yes",
	"auto",
};

enum_names allow_global_redirect_names = {
	GLOBAL_REDIRECT_NO,
	GLOBAL_REDIRECT_AUTO,
	ARRAY_REF(allow_global_redirect_name),
	NULL,
	NULL
};

static const char *const dns_auth_level_name[] = {
	"PUBKEY_LOCAL",
	"DNSSEC_INSECURE",
	"DNSSEC_SECURE",
};

enum_names dns_auth_level_names = {
	PUBKEY_LOCAL, DNSSEC_ROOF-1,
	ARRAY_REF(dns_auth_level_name),
	NULL, /* prefix */
	NULL
};

/*
 * enum sa_type -> enum ike_version
 */

static const char *const ike_version_child_sa_name[] = {
	[IKEv1-IKEv1] = "IPsec SA",
	[IKEv2-IKEv1] = "Child SA"
};

enum_names ike_version_child_sa_names = {
	IKEv1, IKEv2,
	ARRAY_REF(ike_version_child_sa_name),
	NULL, NULL,
};

static const char *const ike_version_parent_sa_name[] = {
	[IKEv1-IKEv1] = "ISAKMP SA",
	[IKEv2-IKEv1] = "IKE SA"
};

enum_names ike_version_parent_sa_names = {
	IKEv1, IKEv2,
	ARRAY_REF(ike_version_parent_sa_name),
	NULL, NULL,
};

static const enum_names *sa_type_ike_version_sa_name[] = {
	[IKE_SA] = &ike_version_parent_sa_names,
	[IPSEC_SA] = &ike_version_child_sa_names,
};

enum_enum_names sa_type_ike_version_sa_names = {
	SA_TYPE_FLOOR, SA_TYPE_ROOF-1,
	ARRAY_REF(sa_type_ike_version_sa_name),
};

/* enum kernel_policy_op_names */

static const char *kernel_policy_op_name[] = {
	[0] = "KP_INVALID",
#define S(E) [E] = #E
	S(KP_ADD_OUTBOUND),
	S(KP_REPLACE_OUTBOUND),
	S(KP_DELETE_OUTBOUND),
	S(KP_ADD_INBOUND),
	S(KP_REPLACE_INBOUND),
	S(KP_DELETE_INBOUND),
#undef S
};

enum_names kernel_policy_op_names = {
	0, elemsof(kernel_policy_op_name)-1,
	ARRAY_REF(kernel_policy_op_name),
	.en_prefix = "KP_",
};

/* */

static const char *const perspective_name[] = {
	[NO_PERSPECTIVE] = "NO_PERSPECTIVE",
	[LOCAL_PERSPECTIVE] = "LOCAL_PERSPECTIVE",
	[REMOTE_PERSPECTIVE] = "REMOTE_PERSPECTIVE"
};

enum_names perspective_names = {
	NO_PERSPECTIVE, REMOTE_PERSPECTIVE,
	ARRAY_REF(perspective_name),
	NULL, /* prefix */
	NULL,
};

static const char *const shunt_policy_name[] = {
#define A(S) [S] = #S
	A(SHUNT_UNSET),
	A(SHUNT_HOLD),
	A(SHUNT_NONE),
	A(SHUNT_PASS),
	A(SHUNT_DROP),
	A(SHUNT_REJECT),
	A(SHUNT_TRAP),
#undef A
};

enum_names shunt_policy_names = {
	SHUNT_UNSET, SHUNT_POLICY_ROOF-1,
	ARRAY_REF(shunt_policy_name),
	"SHUNT_", /* prefix */
	NULL,
};

/* print a policy: like bitnamesof, but it also does the non-bitfields.
 * Suppress the shunt and fail fields if 0.
 */

size_t jam_policy(struct jambuf *buf, lset_t policy)
{
	size_t s = 0;

	if (policy != LEMPTY) {
		s += jam_lset_short(buf, &sa_policy_bit_names, "+", policy);
	}
	return s;
}

const char *str_policy(lset_t policy, policy_buf *dst)
{
	struct jambuf buf = ARRAY_AS_JAMBUF(dst->buf);
	jam_policy(&buf, policy);
	return dst->buf;
}

static const enum_names *pluto_enum_names_checklist[] = {
	&kern_interface_names,
	&dpd_action_names,
	&sd_action_names,
	&natt_method_names,
	&routing_story,
	&stf_status_names,
#ifdef XFRM_SUPPORT
	&netkey_sa_dir_names,
#endif
	&ike_version_parent_sa_names,
	&ike_version_child_sa_names,
	&perspective_names,
	&sa_policy_bit_names,
	&kernel_policy_op_names,
	&shunt_policy_names,
};

void init_pluto_constants(void) {
	check_enum_names(ARRAY_REF(pluto_enum_names_checklist));
}
