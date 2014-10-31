#!/bin/sh
#chkconfig: 0123456 87 14
# description: ipv4_log_v1
# processname: ipv4_log
# config: /usr/local/ipv4_log/etc/ipv4_log.conf

PROGRAM=ipv4_log
PROG_PATH=/usr/local/ipv4_log
CAT=$PROG_PATH/bin/ipv4_log
CONF=$PROG_PATH/etc/config.txt

RETVAL=0
# Source function library.
if [ -f /etc/init.d/functions ]; then
  . /etc/init.d/functions
elif [ -f /etc/rc.d/init.d/functions ] ; then
  . /etc/rc.d/init.d/functions
else
  exit 0
fi
if [ -f  /etc/profile ]; then
. /etc/profile
fi

# NOTE: remove blank characters in front of or at the back of line in config file
sed -i 's/^[[:blank:]]*//g' $CONF
sed -i 's/[[:blank:]]*$//g' $CONF

which service > /dev/null 2>&1
if [ $? -ne 0 ];then
	export PATH=$PATH:/sbin
fi

debug_out()
{
	echo -n $2
	if [ $1 == 0 ];then
		echo_success
		echo
	elif [ $1 == 1 ];then
		echo_failure
		echo
		exit 1
	else
		echo_warning
		echo
	fi
}

parse_config_in_section()
{
        local section=$1
        local line_count=$2
        local key=$3
        local spliter="="
        grep -A${line_count} "\[${section}\]" $CONF | grep "^${key}${spliter}" | grep -v "^#" | awk -F "${spliter}" '{print $2}' | grep -o "[^ ]\+\( \+[^ ]\+\)*"
}

parse_config()
{
	local key=$1
	local spliter="="
	cat $CONF | grep "^${key}${spliter}" | grep -v "^#" | awk -F "${spliter}" '{print $2}' | grep -o "[^ ]\+\( \+[^ ]\+\)*"
}

if [ ! -d $PROG_PATH ];then
	echo "ipv4 log is not installed!"
	exit 1
fi

if [ ! -f $CONF ];then
	echo "ipv4_log configure file not found: $CONF "
	exit 1
fi
if [ ! -f $CAT ];then
        echo "ipv4_log program file not found: $CAT "
    	exit 1
fi


check_environment()
{
	#Check kernel version
	if [ ! -f /lib/modules/`uname -r`/kernel/net/ipv4/netfilter/ipv4_nf_log.ko ];then
		debug_out 1 "The kernel module(ipv4_nf_log.ko) not exist!"
	else
		if [ ! -f /lib/modules/`uname -r`/kernel/net/ipv4/netfilter/nf_conntrack_ipv4.ko ];then
			debug_out 1 "The kernel module(nf_conntrack_ipv4.ko) not exist!"
		fi
	fi
	debug_out 0 "environment check:"
}

check_still_running()
{
	local num=`lsmod | grep ipv4_nf_log | wc -l`
	if [ $num -gt 0 ];then
		echo 0 > /proc/sys/net/netfilter/nf_conntrack_stat_enable > /dev/null
		debug_out 0 "Kernel disable:"
	fi
	local cat_num=`ps aux | grep $CAT | grep -v grep | grep -v vi |wc -l`
	if [ $cat_num -gt 0 ];then
		killproc $CAT
		debug_out $? "Stopping cat_line:"
	fi

}

ins_dep_module()
{
	local num=`lsmod | grep nf_conntrack_ipv4 | wc -l`
	if [ $num -lt 1 ];then
		modprobe nf_conntrack_ipv4 > /dev/null
	fi
}

rm_dep_module()
{
	local num=`lsmod | grep nf_conntrack_ipv4 | wc -l`
	if [ $num -ge 1 ];then
		local used_by=`lsmod | grep nf_conntrack_ipv4 | head -n 1 | awk '{print $3}'`
		if [ $used_by -eq 0 ];then
			rmmod nf_conntrack_ipv4 > /dev/null
		fi
	fi
}

rename_old_logfile()
{
	#original log file configure
	local num=`parse_config_in_section original_log 8 max_num`
	local log_file=`parse_config_in_section original_log 8 file`
	local index=0

	if [ -f $log_file.$num ];then
                rm -f $log_file.$num
        fi
	
	while (($num >= 0))
	do
		if [ -f $log_file.$num ];then
			index=$(($num+1))
			mv $log_file.$num $log_file.$index
		fi
		num=$(($num-1))
	done
	#stat log 
	num=`parse_config_in_section stat_log 8 max_num`
        log_file=`parse_config_in_section stat_log 8 file`
        index=0
	if [ -f $log_file.$num ];then
		rm -f $log_file.$num
	fi
        while (($num >= 0))
        do
                if [ -f $log_file.$num ];then
                        index=$(($num+1))
                        mv $log_file.$num $log_file.$index
                fi
                num=$(($num-1))
        done


}

config_start()
{
	local cfg_conn_max=`parse_config ip_conntrack_max`
	if [ ! -f /proc/sys/net/ipv4/netfilter/ip_conntrack_max ];then
		debug_out 1 "The proc (/proc/sys/net/ipv4/netfilter/ip_conntrack_max) not exist!"
	fi
	local sys_conn_max=`cat /proc/sys/net/ipv4/netfilter/ip_conntrack_max`
	if [ $cfg_conn_max -gt $sys_conn_max ];then
		echo $cfg_conn_max > /proc/sys/net/ipv4/netfilter/ip_conntrack_max 
	fi
	local expire=`parse_config expire`
	if [ $expire -lt 1 ];then
		debug_out 2 "Invalid config(expire),use default value(60) instead!"
		expire=60
	fi
	echo $expire > /proc/sys/net/netfilter/nf_conntrack_stat_expire
	
	#backup the existed log file
	#rename_old_logfile
	#start the program
	nohup $CAT > /dev/null 2>&1 &
	sleep 1
	local prog_num=`ps aux | grep $CAT | grep -v grep | grep -v vi | wc -l`
	if [ $prog_num -eq 0 ];then
		debug_out 1 "Starting ipv4 log:"
	fi
	debug_out 0 "Starting ipv4 log:"
	
	echo 1 > /proc/sys/net/ipv4/ipv4_log
	echo 1 > /proc/sys/net/netfilter/nf_conntrack_stat_enable 

	debug_out 0 "Kernel enable:"
}

start()
{
	check_environment
	check_still_running

	ins_dep_module
	modprobe ipv4_nf_log > /dev/null 2>&1
	config_start
}

stop()
{
	check_still_running
	
	local num=`lsmod | grep ipv4_nf_log | wc -l`
	if [ $num -gt 0 ];then
		rmmod ipv4_nf_log > /dev/null 2>&1
	fi
	rm_dep_module
	
}


restart()
{
	stop
	sleep 3
	start
}

rhstatus()
{
	local num=`lsmod | grep ipv4_nf_log | wc -l`
	if [ $num -lt 1 ];then
		debug_out 2 "Module ipv4_nf_log was not insmod!"
	fi
	num=`lsmod | grep nf_conntrack_ipv4 | wc -l`
	if [ $num -lt 1 ];then
		debug_out 2 "Module nf_conntrack_ipv4 was not insmod"
	fi

	debug_out 0 "Kernel module status:"
	local enable=`cat /proc/sys/net/ipv4/ipv4_log`
	if [ $enable -eq 0 ];then
		debug_out 2 "Kernel log (ipv4_log) is disabled:"
	else
		enable=`cat /proc/sys/net/netfilter/nf_conntrack_stat_enable`
		if [ $enable -eq 0 ];then
			debug_out 2 "Kernel log (ipv4_nf_log) is disabled:"
		else
			debug_out 0 "Kernel log is enabled:"
		fi
	fi

	local cat_num=`ps aux | grep $CAT | grep -v grep | wc -l`
	if [ $cat_num -lt 1 ];then
		debug_out 2 "The program cat_line is stopped:"
	else
		debug_out 0 "The ipv4 log is running:"
	fi
}



# See how we were called.
case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
	restart)
		restart
		;;
	#reload)
	#	reload
	#	;;
	#condrestart)
	#	condrestart
	#	;;
	status)
		rhstatus
		;;
	*)
		echo $"Usage: $PROGRAM {start|stop|restart|status}"
		RETVAL=1
esac
 
exit $RETVAL 
