#!/bin/sh

base_name=ipv4_log
pkg_dir=ipv4_log
bin_file=$pkg_dir/bin/$base_name
cfg_file=$pkg_dir/etc/$base_name.cfg
service_script=$pkg_dir/etc/$base_name
ins_dir=/usr/local/$base_name


stop_running()
{
	module=`/sbin/lsmod | grep ipv4_nf_log | wc -l`
	if [ $module -gt 0 ];then
		echo 0 > /proc/sys/net/netfilter/nf_conntrack_stat_enable
	fi	

	#check again to avoid missing stop some program (When we change the config file,
	# the log mode for example,when we change to proc from ulogd,then when we stop,
	# we may try to stop cat_line but not ipv4_ulogd in service stop,and then the
	# ipv4_ulogd may still running,we need to stop it here)
	num=`ps aux | grep cat_line | grep -v grep | wc -l`
	if [ $num -gt 0 ];then
		killall cat_line
	fi

}

install_package()
{
	if [ -d $ins_dir ];then
		mv $ins_dir{,.bak_$(date +%Y%m%d-%H:%M)}
	fi
	mkdir $ins_dir
	mkdir $ins_dir/bin
	mkdir $ins_dir/etc
	mkdir $ins_dir/var
	mkdir $ins_dir/var/log

	cp $bin_file $ins_dir/bin
	cp $cfg_file $ins_dir/etc
	cp $service_script $ins_dir/etc
	chmod +x $ins_dir/bin/*

	if [ -f /etc/init.d/$base_name ];then
		/sbin/chkconfig $base_name off
		rm -f /etc/init.d/$base_name
	fi
	ln -s $ins_dir/etc/$base_name /etc/init.d/$base_name
	chmod +x /etc/init.d/$base_name
	/sbin/chkconfig $base_name on
}

install_check()
{
	if [ ! -d $ins_dir ];then
		echo "$base_name install fail!"
	fi
	if [ ! -x $ins_dir/bin/$base_name ];then
		echo "$base_name install fail!"
		rm -rf $ins_dir
	fi
	if [ ! -x /etc/init.d/$base_name ];then
		echo "$base_name install fail!"
		/sbin/chkconfig $base_name off
		if [ -f /etc/init.d/$base_name ];then
			rm -f /etc/init.d/$base_name
		fi
		if [ -d $ins_dir ];then
			rm -rf $ins_dir
		fi
	fi
	echo "$base_name install successful!"
}

uninstall_package()
{
	/sbin/chkconfig $base_name off
	rm -f /etc/init.d/$base_name
	rm -rf $ins_dir
}

install()
{
	if [ -d ./$pkg_dir ];then
		rm -rf $pkg_dir
	fi
	tar zxf $pkg_dir.tar.gz
	
	stop_running
	install_package
	install_check
	
	rm -rf $pkg_dir
}

uninstall()
{
	stop_running
	uninstall_package
}

print_help()
{
	echo -e "Usage: $0
	-h \t help
	-i \t install the $base_name package
	-e \t uninstall the $base_name package"
        exit 0;

}

if [ $# -ne 1 ];then
        print_help
fi
while [ -n "$1" ]
do
        case $1 in
                -h) print_help; shift;;
                -i) install; shift;;
                -e) uninstall; shift;;
                *) print_help; shift;;
        esac
done
exit 0




