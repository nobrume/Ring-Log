#pragma warning(disable:4996)
#ifndef __RING_LOG_H__
#define __RING_LOG_H__

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include<new>
// 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
#define EPOCHFILETIME   (116444736000000000UL)
 
//#include <sys/time.h>
//#include <sys/types.h>//getpid, gettid


enum LOG_LEVEL
{
    FATAL = 1,
    ERROR1,//ERROR is crashed with <windows.h> 
    WARN,
    INFO,
    DEBUG,
    TRACE,
};
typedef long long int64t;

extern pid_t gettid();
extern	int pExit;

struct utc_timer
{
    utc_timer()
    {
//        struct timeval tv;
//        gettimeofday(&tv, NULL);
		FILETIME ft;
		LARGE_INTEGER li;
		int64_t tt = 0;
		GetSystemTimeAsFileTime(&ft);
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
		tt = (li.QuadPart - EPOCHFILETIME) / 10;
		_sys_acc_sec = tt / 1000000;
		_sys_acc_min = _sys_acc_sec / 60;

        //use _sys_acc_sec calc year, mon, day, hour, min, sec
        struct tm cur_tm;
		localtime_s(&cur_tm ,(time_t*)&_sys_acc_sec );
		year = cur_tm.tm_year+1900;
		mon = cur_tm.tm_mon+1;
		day = cur_tm.tm_mday;
		hour = cur_tm.tm_hour;
		min = cur_tm.tm_min;
		sec = cur_tm.tm_sec;
        reset_utc_fmt();
    }

    uint64_t get_curr_time(int* p_msec = NULL)
    {
        //struct timeval tv;
        //get current ts
      //  gettimeofday(&tv, NULL);
		FILETIME ft;
		LARGE_INTEGER li;
		int64_t tt = 0;
		GetSystemTimeAsFileTime(&ft);
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
		tt = (li.QuadPart - EPOCHFILETIME) / 10;
		if (p_msec) *p_msec = (int)((tt / 1000)%1000);
		tt = tt / 1000000;//转换成秒
        //if not in same seconds
        if (/*(uint32_t)*/tt != _sys_acc_sec)
        {
            sec = tt % 60;
            _sys_acc_sec = tt;
            //or if not in same minutes
            if (_sys_acc_sec / 60 != _sys_acc_min)
            {
                //use _sys_acc_sec update year, mon, day, hour, min, sec
                _sys_acc_min = _sys_acc_sec / 60;
                struct tm cur_tm;
				localtime_s(&cur_tm, (time_t*)&_sys_acc_sec);
                year = cur_tm.tm_year + 1900;
                mon  = cur_tm.tm_mon + 1;
                day  = cur_tm.tm_mday;
                hour = cur_tm.tm_hour;
                min  = cur_tm.tm_min;
                //reformat utc format
                reset_utc_fmt();
            }
            else
            {
                //reformat utc format only sec
                reset_utc_fmt_sec();
            }
        }
        return tt;
    }

    int year, mon, day, hour, min, sec;
    char utc_fmt[20];

private:
    void reset_utc_fmt()
    {
        sprintf_s(utc_fmt,"%d-%02d-%02d %02d:%02d:%02d", year, mon, day, hour, min, sec);
    }
    
    void reset_utc_fmt_sec()
    {
        sprintf(utc_fmt + 17,  "%02d", sec);
    }

    uint64_t _sys_acc_min;
    uint64_t _sys_acc_sec;
};

class cell_buffer
{
public:
    enum buffer_status
    {
        FREE,
        FULL
    };

    cell_buffer(uint32_t len): 
    status(FREE), 
    prev(NULL), 
    next(NULL), 
    _total_len(len), 
    _used_len(0)
    {
		pExit = 0;
		_data = new char[len];//offen no memory in vs2013 for win10
//		_data = new (std::nothrow) char[len];// 抑制异常抛出，返回空指针
	
		if (!_data)
		{
			fprintf(stderr, "no space to allocate _data\n");
			pExit = 1;//exit
			exit(1);
		}

    }
    uint32_t avail_len() const { return _total_len - _used_len; }

    bool empty() const { return _used_len == 0; }

    void append(const char* log_line, uint32_t len)
    {
        if (avail_len() < len)
            return ;
        memcpy(_data + _used_len, log_line, len);
        _used_len += len;
    }

    void clear()
    {
        _used_len = 0;
        status = FREE;
    }

    void persist(FILE* fp)
    {
        uint32_t wt_len = fwrite(_data, 1, _used_len, fp);
        if (wt_len != _used_len)
        {
            fprintf(stderr, "write log to disk error, wt_len %u\n", wt_len);
        }
    }

    buffer_status status;

    cell_buffer* prev;
    cell_buffer* next;

private:
    cell_buffer(const cell_buffer&);
    cell_buffer& operator=(const cell_buffer&);

    uint32_t _total_len;
    uint32_t _used_len;
    char* _data;
};

class ring_log
{
public:
    //for thread-safe singleton
    static ring_log* ins()
    {
        pthread_once(&_once, ring_log::init);
        return _ins;
    }

    static void init()
    {
        while (!_ins) _ins = new ring_log();
    }

    void init_path(const char* log_dir, const char* prog_name, int level);

    int get_level() const { return _level; }

    void persist();

    void try_append(const char* lvl, const char* format, ...);
	
private:
    ring_log();

    bool decis_file(int year, int mon, int day);

    ring_log(const ring_log&);
    const ring_log& operator=(const ring_log&);

    int _buff_cnt;

    cell_buffer* _curr_buf;
    cell_buffer* _prst_buf;

    cell_buffer* last_buf;

    FILE* _fp;
    pid_t _pid;
    int _year, _mon, _day, _log_cnt;
    char _prog_name[128];
    char _log_dir[512];

    bool _env_ok;//if log dir ok
    int _level;
    uint64_t _lst_lts;//last can't log error time(s) if value != 0, log error happened last time
    
    utc_timer _tm;

    static pthread_mutex_t _mutex;
    static pthread_cond_t _cond;

    static uint32_t _one_buff_len;

    //singleton
    static ring_log* _ins;
    static pthread_once_t _once;


};


extern  pthread_t  thdo_tid;
void* be_thdo(void* args);

#define LOG_MEM_SET(mem_lmt) \
    do \
			    { \
        if (mem_lmt < 90 * 1024 * 1024) \
						        { \
            mem_lmt = 90 * 1024 * 1024; \
						        } \
														        else if (mem_lmt > 1024 * 1024 * 1024) \
        { \
            mem_lmt = 1024 * 1024 * 1024; \
        } \
        ring_log::_one_buff_len = mem_lmt; \
			    } while (0)

#define LOG_INIT(log_dir, prog_name, level) \
    do \
			    { \
        ring_log::ins()->init_path(log_dir, prog_name, level); \
        pthread_create(&thdo_tid, NULL, be_thdo, NULL); \
			    } while (0)
//        pthread_detach(thdo_tid); 
#define LOG_DETACH() \
    do \
			    { \
		pExit = 1;\
        pthread_join(thdo_tid,NULL); \
				    } while (0)

//format: [LEVEL][yy-mm-dd h:m:s.ms][tid]file_name:line_no(func_name):content
#define LOG_TRACE(fmt, ...) \
    do \
		    { \
        if (ring_log::ins()->get_level() >= TRACE) \
				        { \
            ring_log::ins()->try_append("[TRACE]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
				        } \
		    } while (0)

#define LOG_DEBUG(fmt, ...) \
    do \
		    { \
        if (ring_log::ins()->get_level() >= DEBUG) \
				        { \
            ring_log::ins()->try_append("[DEBUG]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
				        } \
		    } while (0)

#define LOG_INFO(fmt, ...) \
    do \
		    { \
        if (ring_log::ins()->get_level() >= INFO) \
				        { \
            ring_log::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
				        } \
		    } while (0)

#define LOG_NORMAL(fmt, ...) \
    do \
		    { \
		va_list args;va_start(args,fmt);\
        if (ring_log::ins()->get_level() >= INFO) \
				        { \
            ring_log::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
				        } \
		    } while (0)

#define LOG_WARN(fmt, ...) \
    do \
		    { \
        if (ring_log::ins()->get_level() >= WARN) \
				        { \
            ring_log::ins()->try_append("[WARN]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
				        } \
		    } while (0)

#define LOG_ERROR(fmt,...) \
    do \
		    { \
        if (ring_log::ins()->get_level() >= ERROR1) \
		        { \
            ring_log::ins()->try_append("[ERROR]", "[%u]%s:%d(%s): " fmt "\n", \
                gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
		        } \
    } while (0)

#define LOG_FATAL(fmt, ...) \
    do \
    { \
        ring_log::ins()->try_append("[FATAL]", "[%u]%s:%d(%s): " fmt "\n", \
            gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    } while (0)

#define TRACE(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= TRACE) \
        { \
            ring_log::ins()->try_append("[TRACE]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define DEBUG(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= DEBUG) \
        { \
            ring_log::ins()->try_append("[DEBUG]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define INFO(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= INFO) \
        { \
            ring_log::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define NORMAL(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= INFO) \
        { \
            ring_log::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define WARN(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= WARN) \
        { \
            ring_log::ins()->try_append("[WARN]", "[%u]%s:%d(%s): " fmt "\n", \
                    gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define ERROR(fmt, ...) \
    do \
    { \
        if (ring_log::ins()->get_level() >= ERROR1) \
        { \
            ring_log::ins()->try_append("[ERROR]", "[%u]%s:%d(%s): " fmt "\n", \
                gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        } \
    } while (0)

#define FATAL(fmt, ...) \
    do \
    { \
        ring_log::ins()->try_append("[FATAL]", "[%u]%s:%d(%s): " fmt "\n", \
            gettid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    } while (0)

#endif
