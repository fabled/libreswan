#sleep 30; # enable to get time to attach ip xfrm monitor
# trigger a private-or-clear and check for shunt and shunt expiry
../../guestbin/ping-once.sh --forget -I 192.1.3.209 192.1.2.23
# wait on OE to start: should show nothing in shuntstatus (shunt is
# not bare, but with conn), should show up in xfrm policy and show
# partial STATE
../../guestbin/wait-for.sh --match '#1:.*sent IKE_SA_INIT request' -- ipsec status
ipsec whack --shuntstatus
ip -o xfrm pol | grep 192.1.2.23
# wait on OE to fail: should show pass in shuntstatus and xfrm policy
# and without partial STATE
../../guestbin/wait-for.sh --match oe-failing -- ipsec shuntstatus
ip -o xfrm pol | grep 192.1.2.23
ipsec status | grep STATE_ || true
# wait on OE shunt to expire: should show no more shunts for
# 192.1.2.23, no xfrm policy and no STATE's
../../guestbin/wait-for.sh --timeout 60 --no-match oe-failing -- ipsec shuntstatus
ip -o xfrm pol | grep 192.1.2.23 || true
ipsec status | grep STATE_ || true
# repeat test with a hold shunt - but it really shouldn't matter
# trigger a private and check for shunt and shunt expiry
../../guestbin/ping-once.sh --forget -I 192.1.3.209 192.1.3.46
# wait on OE to start: should show nothing in shuntstatus (shunt is
# not bare, but with conn), should show nothing in xfrm policy because
# SPI_HOLD (drop) is a no-op for XFRM as the larval state causes it
# already and should show show partial STATE
../../guestbin/wait-for.sh --match '#2:.*sent IKE_SA_INIT request' -- ipsec status
ipsec whack --shuntstatus
ip -o xfrm pol | grep 192.1.3.46 || true
# wait for OE to fail: should show pass in shuntstatus and xfrm policy
# and without partial STATE
../../guestbin/wait-for.sh --match oe-failed -- ipsec shuntstatus
ip -o xfrm pol | grep 192.1.3.46 || true
ipsec status | grep STATE_ || true
# wait for failing shunt to expire: should show no more shunts for
# 192.1.3.46, no xfrm policy and no STATE's
../../guestbin/wait-for.sh --timeout 60 --no-match oe-failing -- ipsec shuntstatus
ip -o xfrm pol | grep 192.1.3.46 || true
ipsec status | grep STATE_ || true
