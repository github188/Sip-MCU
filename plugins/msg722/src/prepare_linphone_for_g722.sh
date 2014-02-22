#!/bin/sh

# *******************************************************************************************
# * Shell script to change the Linphone code a little bit in order to be able to accept G.722
# * Created by Simon Brenner
# * last updated: 30.07.2010
# *******************************************************************************************

# check correct number of arguments
if [ $# -ne 1 ]
then
	echo "Usage: sh prepare_linphone_for_g722.sh <path-to-linphone-sources>"
else
	plin=$1
	echo "Linphone source dir: $plin"
	cd $plin
	sed -i 's|pt->clock_rate=atoi(p);|pt->clock_rate = (!strcasecmp(mime,"G722") ? 16000 : atoi(p));|' coreapi/sal_eXosip2_sdp.c
	echo "sed patch applied"
fi
