#! /bin/sh
#
# This shell script should be run by CRON or ATRUN daily to extract
# 
#
cd /usr/tinymud/lib

ADDR=$USER
LOGFILE=tinymud.log
GRIPELOG=tinymud.gripes
TMPFILE=/tmp/allgripes${$}
NEWGRIPES=/tmp/newgripes${$}

#
# Long version of logs
#
#egrep 'GRIPE from|FORCE:|WIZARD:|BOBBLE:|BOOT:|SHUTDOWN:|ROBOT:' ${LOGFILE} | sort > ${TMPFILE}
#
# Short version of logs
#
egrep 'GRIPE from|WIZARD:|SHUTDOWN:' ${LOGFILE} | sort > ${TMPFILE}

sort -o ${GRIPELOG} ${GRIPELOG}

comm -13 ${GRIPELOG} ${TMPFILE} > ${NEWGRIPES}

if test -s ${NEWGRIPES}
then
	awk '{print $0;printf "\n"}' ${NEWGRIPES} | \
		fmt | Mail -s "Daily TinyMUD gripes" ${ADDR}
	cat ${NEWGRIPES} >> ${GRIPELOG}
fi

rm -f ${TMPFILE} ${NEWGRIPES}
