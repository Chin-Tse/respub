#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

/*For inet_addr*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>
#include <unistd.h>

#include "ipv4_log.h"
#include "ipv4_log_mem.h"



char stat_file[32] = {'\0'};
FILE *stat_fd = NULL;
char log_file[32] = {'\0'};
FILE *log_fd = NULL;

int stat_file_max_cnt = STAT_LOG_FILE_MAX_CNT;
long stat_file_max_size = STAT_LOG_FILE_MAX_SIZE; /*1G*/
int log_file_max_cnt = STAT_LOG_FILE_MAX_CNT;
long log_file_max_size = STAT_LOG_FILE_MAX_SIZE;
int expire = STAT_DEF_EXPIRE;
time_t g_time;

extern struct ipv4_log_stat_s tcp_log_stat;
extern struct ipv4_log_stat_s udp_log_stat;
extern struct ipv4_log_stat_s icmp_log_stat;

void log_out(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	va_start(ap, fmt);
	vsnprintf(buf,1024,fmt,ap);
	if(stat_fd != NULL)
	{
		fprintf(stat_fd,"%s",buf);
	}
	printf("%s",buf);
	va_end(ap);
	fflush(stat_fd);
}
//#define log_out(fmt, args...) do{\
//	if(stat_fd != NULL) \
//		fprintf(stat_fd,fmt,##args);\
//	printf(fmt,##args); \
//}while(0)



int ipv4_log_parse(char *line,struct log_info *info)
{
	char log_items[ITEM_MAX][MAX_ITEM_LEN] = {{'\0'}};
	char index = 0;
	char *item = NULL;

	item = strtok(line,",");
	while((NULL != item) && (index < ITEM_MAX))
	{
		strncpy((char *)&log_items[index],item,MAX_ITEM_LEN);
		index++;
		item = strtok(NULL,",");
	}
	if(index != ITEM_MAX)
	{
		printf("error here index= %d != %d\n",index,ITEM_MAX);
		return -1;
	}
	info->protocol = (uint8_t)atoi(log_items[ITEM_PROTOCOL_NUM]);
	
	info->src = inet_addr(log_items[ITEM_SRC_IP]);
	info->dst = inet_addr(log_items[ITEM_DST_IP]);
	info->sport = (uint16_t)atoi(log_items[ITEM_SRC_PORT]);
	info->dport = (uint16_t)atoi(log_items[ITEM_DST_PORT]);
	
	info->stat.snd_p = atol(log_items[ITEM_SND_PKTS]);
	info->stat.rcv_p = atol(log_items[ITEM_RCV_PKTS]);
	info->stat.snd_b = atoll(log_items[ITEM_SND_BYTES]);
	info->stat.rcv_b = atoll(log_items[ITEM_RCV_BYTES]);
	info->stat.syn = atol(log_items[ITEM_SYN_CNT]);
	info->stat.synack = atol(log_items[ITEM_SYNACK_CNT]);
	
	info->code = (uint8_t)atoi(log_items[ITEM_CODE_CNT]);
	info->o_type = (uint8_t)atoi(log_items[ITEM_TYPE_O]);
	info->o_type = (uint8_t)atoi(log_items[ITEM_TYPE_R]);
	
	strncpy(info->if_name,log_items[ITEM_DEV],MAX_ITEM_LEN);
	
	return 0;

}

int rename_stat_file(void)
{
	int i;
	char cmd[128];
	int len = 0;
	static int stat_file_cnt = 0;
	if(stat_file_cnt == stat_file_max_cnt)
	{
		len = snprintf(cmd,128,"rm -f %s.%d ",stat_file,stat_file_cnt);
		cmd[len] = '\0';
		system(cmd);
		stat_file_cnt--;
	}
	for (i = stat_file_cnt;i >= 0;i--)
	{
		len = snprintf(cmd,128,"mv %s.%d %s.%d ",stat_file,i,stat_file,i+1);
		cmd[len] = '\0';
		system(cmd);
	}
	char tmp[64] = {'\0'};
	FILE *tmp_file = NULL;
	snprintf(tmp,64,"%s.0",stat_file);
	tmp_file = fopen(tmp,"wb");
	if(tmp_file == NULL)
	{
		printf("exit 1\n");
		return -1;
	}
	fclose(stat_fd);
	stat_fd = tmp_file;
	stat_file_cnt++;
	return 0;
}



int rename_log_file(void)
{
    int i;
    char cmd[128];
    int len = 0;
	static int log_file_cnt = 0;
    if(log_file_cnt == log_file_max_cnt)
    {
            len = snprintf(cmd,128,"rm -f %s.%d ",log_file,log_file_cnt);
            cmd[len] = '\0';
            system(cmd);
            log_file_cnt--;
    }
    for (i = log_file_cnt;i >= 0;i--)
	{
    len = snprintf(cmd,128,"mv %s.%d %s.%d ",log_file,i,log_file,i+1);
					cmd[len] = '\0';
					system(cmd);
	}
	char tmp[64] = {'\0'};
	FILE *tmp_file = NULL;
	snprintf(tmp,64,"%s.0",log_file);
	tmp_file = fopen(tmp,"wb");
	if(tmp_file == NULL)
	{
		printf("exit 1\n");
		return 1;
	}
	fclose(log_fd);
	log_fd = tmp_file;
	log_file_cnt++;
	return 0;
}

int ipv4_log_check_time(void)
{
	time_t crt_time;
	crt_time = time(NULL);
	if(crt_time - g_time >= expire)
	{
		g_time = crt_time;
		return 0;
	}
	return -1;
}

void ipv4_log_time_out(void)
{
	/*Maybe we can process this in child thread*/
	static long len = 0;
	time_t crt_time = time(NULL);
	struct tm * crt_tm = localtime(&crt_time);
	log_out("\n==============%04d%02d%02d %02d:%02d:%02d=====================\n",
		crt_tm->tm_year + 1900, crt_tm->tm_mon + 1, crt_tm->tm_mday,
		crt_tm->tm_hour, crt_tm->tm_min, crt_tm->tm_sec);
	len += 37;
	log_out("[TCP]\n");
	len += (unsigned long)tcp_log_stat.time_out(&tcp_log_stat.list);
	log_out("[UDP]\n");
	len += (unsigned long)udp_log_stat.time_out(&udp_log_stat.list);
	log_out("[ICMP]\n");
	len += (unsigned long)icmp_log_stat.time_out(&icmp_log_stat.list);
	if(len >= stat_file_max_size)
	{
		rename_stat_file();
		len = 0;
	}
	return;
}
				
int ipv4_log_stat(FILE* file)
{
	char buf[1024] = {0};
	int len;
	static long total_len;
	struct log_info info;

	while(fgets(buf,1024,file) != NULL)
	{
		total_len += strlen(buf)+1;
		fprintf(log_fd,"%s",buf);
		if(total_len >= log_file_max_size)
		{
			if(rename_log_file() == 0)
				total_len=0;
		}
		if(ipv4_log_parse(buf,&info) == 0)
		{
			switch(info.protocol)
			{

			case IPPROTO_TCP:
				if(LOG_ACCEPT == (tcp_log_stat.log_match(&info)))
				{
					tcp_log_stat.log_stat(&info);
				}
				break;
			case IPPROTO_UDP:
				if(LOG_ACCEPT == (udp_log_stat.log_match(&info)))
				{
					udp_log_stat.log_stat(&info);
				}
				break;
			case IPPROTO_ICMP:
				if(LOG_ACCEPT == (icmp_log_stat.log_match(&info)))
				{
					icmp_log_stat.log_stat(&info);
				}
				break;
			default:
				break;
			}
		}
		if(0 == ipv4_log_check_time())
		{
			ipv4_log_time_out();
		}
	}

}
void ipv4_log_help(void)
{
		printf("Usage for ipv4_log: ipv4_log [-w log] [-s size] [-c cnt] [-n] [-f file] [-h]\n");
		printf("\tw file\tsave the stat result to log file\n");
		printf("\tW file\tsave the original log to log file\n");
		printf("\ts size\tset the max size of each log file,default is 100M\n");
		printf("\tc cnt\tset the max num of log_files will be keep,default is 10\n");
		printf("\tf file\tthe file to cat\n");
		printf("\th or ?\tshow this usage\n");
		return;
}
				
int main(int argc,char* argv[])
{
	int i = 0 ;
	int j = 0;
	int file_exist = 0;
	FILE* file = NULL;
	char tmp[64] = {'\0'};
	
	int c;
	while((c = getopt(argc, argv, "w:W:c:s:f:h?")) != -1)
	{
		switch(c)
		{
		case 'w':
			strncpy(stat_file,optarg,31);
			snprintf(tmp,64,"%s.0",stat_file);
			stat_fd = fopen(tmp,"wb");
			if(NULL == stat_fd){
				fprintf(stderr,"open %s fail!\n",stat_file);
				return -1;
			}
			break;
		case 'W':
			strncpy(log_file,optarg,31);
			snprintf(tmp,64,"%s.0",stat_file);
			log_fd = fopen(tmp,"wb");
			if(NULL == log_fd){
				fprintf(stderr,"open %s fail!\n",log_file);
				return -1;
			}
			break;

		case 's':
			stat_file_max_size = atol(optarg);
			break;

		case 'c':
			stat_file_max_cnt = atoi(optarg);
			break;
		case 'f':
			file_exist = 1;
			file = fopen(optarg,"rb");
			if(file == NULL)
			{
				fprintf(stderr,"%s:err reading from %s:%s\n",
							argv[0],argv[j],strerror(errno));
				return -1;
			}
			break;
		case '?':
		case 'h':
		default:
			ipv4_log_help();
			return 0;

		}
	}
	/*If the stat_file is not setted,set to default*/
	if(!file_exist)
	{
		file = fopen(STAT_LOG_DEF_FILE,"rb");
		if(file == NULL)
		{
			fprintf(stderr,"%s:err reading from %s:%s\n",
						argv[0],argv[j],strerror(errno));
			return -1;
		}
	}
	/*If the stat log file is not setted,set to default*/
	if(stat_fd == NULL)
	{
		strncpy(stat_file,DEF_STAT_FILE,31);
		snprintf(tmp,64,"%s.0",DEF_STAT_FILE);
		stat_fd = fopen(tmp,"wb");
		if(NULL == stat_fd){
			fprintf(stderr,"open %s fail!\n",DEF_STAT_FILE);
			return -1;
		}
	}
	if(log_fd == NULL)
	{
		strncpy(log_file,DEF_LOG_FILE,31);
		snprintf(tmp,64,"%s.0",DEF_LOG_FILE);
		log_fd = fopen(tmp,"wb");
		if(NULL == log_fd){
			fprintf(stderr,"open %s fail!\n",DEF_LOG_FILE);
			return -1;
		}
	}

	if(create_mem_stack() < 0)
		return -1;
	INIT_LIST_HEAD(&tcp_log_stat.list);	
	INIT_LIST_HEAD(&udp_log_stat.list);	
	INIT_LIST_HEAD(&icmp_log_stat.list);	

	g_time = time(NULL);
	ipv4_log_stat(file);
}


