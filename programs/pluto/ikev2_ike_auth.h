/* IKEv2 IKE_INTERMEDIATE exchange, for libreswan
 *
 * Copyright (C) 2021   Andrew Cagney <cagney@gnu.org>
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

#ifndef IKEV2_IKE_AUTH_H
#define IKEV2_IKE_AUTH_H

stf_status initiate_v2_IKE_AUTH_request(struct ike_sa *ike, struct msg_digest *md);

extern ikev2_state_transition_fn process_v2_IKE_AUTH_request;
extern ikev2_state_transition_fn process_v2_IKE_AUTH_response;
extern ikev2_state_transition_fn process_v2_IKE_AUTH_failure_response;

#endif
