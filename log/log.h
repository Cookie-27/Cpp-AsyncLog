#pragma once

#include<sys/time.h>
#include<stdint.h>
#include<pthread.h>
#include<sys/types.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<string>
#include<chrono>
#include<ctime>
#include<iostream>
#include<sstream>
#include<iomanip>
#include<pthread.h>
#include <sys/syscall.h>
#include <unistd.h>


void* bethdo(void* args);

//日志级别
enum LogLevel{
    TRACE = 1,
    DEBUG,
    INFO,
    ERROR
};

//时间类
class UTCTimer
{
public:
    UTCTimer();
    uint64_t getCurrrntTime(int*);
    
    int year,month,day,hour,min,sec,ms;
    std::string timeFormat;

private:

    void resetFormat();

};

//缓冲单元
class CellBuffer{
public:
    CellBuffer(uint32_t len);

    void persists(FILE* fp);
    void clear();
    void append(const std::string &&addData,uint32_t len);
    bool empty() const;
    uint32_t availableLen() const;

    bool bufferStatus;
    CellBuffer* prev;
    CellBuffer* next;


private:

    uint32_t totalLen;
    uint32_t usedLen;
    char *data;

};

//日志类
class Log{
public:
    static Log* getInstance(){
        pthread_once(&once, Log::init);
        return log;
    }
    static void init(){
        while (!log) log = new Log();
    }


    void initPath(std::string logDir,std::string progName,int level);
    int getLevel() const {return logLevel;}
    void persists();
    bool updateFile(int year,int month,int day);
    void append(const char* lvl,const char *format, ...);
    void* betho();
private:
    Log();  //单例
    Log(const Log&);
    const Log& operator=(const Log&);

private:
    static Log* log;
    static pthread_once_t once;
    static pthread_mutex_t mutex;
    static pthread_cond_t cond;

    int bufCnt;
    CellBuffer* currBuffer;
    CellBuffer* presBuffer;
    CellBuffer* lastBuffer;
    int bufNum;

    static uint32_t perBufLen;

    bool envStatus;
    int year,month,day,logCnt;

    uint64_t lastErrorTime;
    UTCTimer utcTimer;

    FILE* fp;         //写入文件
    pid_t pid;        //进程id
    int logLevel;    //日志的级别
    std::string logDir;  //目录
    std::string progName;  //名称

};



//日志操作的宏定义
#define LOG_INIT(logDir, progName, level) \
    do \
    { \
        Log::getInstance()->initPath(logDir, progName, level); \
        pthread_t tid1; \
        pthread_create(&tid1, NULL, bethdo, NULL); \
        pthread_detach(tid1); \
    } while (0)

#define TRACE(fmt, args...) \
    do \
    { \
        if (Log::getInstance()->getLevel() <= TRACE) \
        { \
            Log::getInstance()->append("[TRACE]", "[%u]%s:%d(%s): " fmt "\n", \
                    getpid(), __FILE__, __LINE__, __FUNCTION__, ##args); \
        } \
    } while (0)

#define DEBUG(fmt, args...) \
    do \
    { \
        if (Log::getInstance()->getLevel() <= DEBUG) \
        { \
            Log::getInstance()->append("[DEBUG]", "[%u]%s:%d(%s): " fmt "\n", \
                    getpid(), __FILE__, __LINE__, __FUNCTION__, ##args); \
        } \
    } while (0)

#define INFO(fmt, args...) \
    do \
    { \
        if (Log::getInstance()->getLevel() <= INFO) \
        { \
            Log::getInstance()->append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
                    getpid(), __FILE__, __LINE__, __FUNCTION__, ##args); \
        } \
    } while (0)

#define ERROR(fmt, args...) \
    do \
    { \
        if (Log::getInstance()->getLevel() <= ERROR) \
        { \
            Log::getInstance()->append("[ERROR]", "[%u]%s:%d(%s): " fmt "\n", \
                getpid(), __FILE__, __LINE__, __FUNCTION__, ##args); \
        } \
    } while (0)
