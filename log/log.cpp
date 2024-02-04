#include "log.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

#define MEM_USE_LIMIT (3u*1024*1024*1024) //3GB
#define LOG_USE_LIMIT (1u*1024*1024*1024) //1GB
#define LOG_LEN_LIMIT (4*1024) //4K
#define RELOG_TTHRESOLD 5
#define BUFF_WAIT_TIME 1

pthread_mutex_t Log::mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Log::cond = PTHREAD_COND_INITIALIZER;

Log* Log::log = NULL;
pthread_once_t Log::once = PTHREAD_ONCE_INIT;
uint32_t Log::perBufLen = 30*1024*1024;//30MB

//UTCTimer Part
UTCTimer::UTCTimer(){
    int ms;
    getCurrrntTime(&ms);
}

uint64_t UTCTimer::getCurrrntTime(int* pmsec = NULL)
{
    auto currentTime = std::chrono::system_clock::now();

    std::time_t currentTime_c = std::chrono::system_clock::to_time_t(currentTime);

    std::tm* utcTime = std::gmtime(&currentTime_c);

    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count() % 1000;

    year = utcTime->tm_year + 1900;
    month = utcTime->tm_mon + 1;
    day = utcTime->tm_mday;
    hour = utcTime->tm_hour;
    min = utcTime->tm_min;
    sec = utcTime->tm_sec;
    ms = milliseconds;

    resetFormat();

    if (pmsec)
        *pmsec = static_cast<int>(milliseconds);

    return utcTime->tm_sec;
}


void UTCTimer::resetFormat(){
    std::ostringstream oss;

    oss << std::setfill('0') << std::setw(4) << year << "-"
        << std::setw(2) << month << "-"
        << std::setw(2) << day << " "
        << std::setw(2) << hour << ":"
        << std::setw(2) << min << ":"
        << std::setw(2) << sec << "."
        << std::setw(3) << ms;

    timeFormat = oss.str();
}


//CellBuffer part
CellBuffer::CellBuffer(uint32_t len):bufferStatus(false),prev(NULL),next(NULL),totalLen(len),usedLen(0)
{
    data  = new char[len];
    if(!data){
        fprintf(stderr,"no space to allocate");
        exit(1);
    }
}

void CellBuffer::clear(){
    usedLen = 0;
    bufferStatus = false;
}

bool CellBuffer::empty() const{
    return usedLen == 0;
}

void CellBuffer::append(const std::string &&addData,uint32_t len){
    if(availableLen() < len){
        return;
    }
    memcpy(data + usedLen,addData.c_str(),len);
    usedLen += len;
}

void CellBuffer::persists(FILE* fp){
    uint32_t len = fwrite(data,1,usedLen,fp);
    if(len != usedLen){
        fprintf(stderr,"write to disk error");
    }
}

uint32_t CellBuffer::availableLen() const{
    return totalLen - usedLen;
}




//Log part
Log::Log():bufNum(3),currBuffer(NULL),presBuffer(NULL),fp(NULL),logLevel(INFO),logCnt(0),envStatus(false),lastErrorTime(0),utcTimer()
{
    CellBuffer* head = new CellBuffer(perBufLen);
    if(!head){
        fprintf(stderr,"no space to allcate Buffer");
        exit(1);
    }
    CellBuffer* current;
    CellBuffer* prev = head;
    for(int i=0;i<bufCnt;i++){
        current = new CellBuffer(perBufLen);
        if(!current){
            fprintf(stderr,"no space to allcate Buffer");
            exit(1);
        }
        current -> prev = prev;
        prev ->next = current;
        prev = current;
    }
    prev-> next = head;
    head-> prev = prev;

    currBuffer = head;
    presBuffer = head;

    pid = getpid();
}

//文件路径的初始化
void Log::initPath(const std::string logDir,const std::string progName,int level){
    
    pthread_mutex_lock(&mutex);
    this->logDir = logDir;
    this->progName = progName;


    mkdir(logDir.c_str(),0777); //读写权限
    
    if(access(logDir.c_str(),F_OK | W_OK) == -1){
        fprintf(stderr,"logDir error");
    }
    else{
        envStatus = true;
    }

    if(level < TRACE){
        level = TRACE;
    }
    if(level > ERROR){
        level = ERROR;
    }
    logLevel = level;
    pthread_mutex_unlock(&mutex);
    std::cout<<"inti end\n";
}

void Log::persists(){
    while(true){
        pthread_mutex_lock(&mutex);

        if(presBuffer->bufferStatus == false){
            struct timespec tsp;
            struct timeval now;
            gettimeofday(&now,NULL);
            tsp.tv_sec = now.tv_sec;
            tsp.tv_nsec = now.tv_usec * 1000;
            tsp.tv_sec += BUFF_WAIT_TIME;
            pthread_cond_timedwait(&cond,&mutex,&tsp);
        }
        if(presBuffer->empty()){
            pthread_mutex_unlock(&mutex);
            continue;
        }
        if(presBuffer->bufferStatus == false){
            currBuffer->bufferStatus = true;
            currBuffer = currBuffer ->next;
        }

        int yer = utcTimer.year,mon = utcTimer.month,da = utcTimer.day;
        pthread_mutex_unlock(&mutex);

        if(!updateFile(yer,mon,da)){
            continue;
        }        
        presBuffer->persists(fp);
        fflush(fp);

        pthread_mutex_lock(&mutex);
        presBuffer->clear();
        presBuffer = presBuffer->next;
        pthread_mutex_unlock(&mutex);

    }
}

bool Log::updateFile(int year,int month,int day){
    if(!envStatus){
        if(fp) fclose(fp);
        fp = fopen("/dev/null","w");
        return fp != NULL;
    }
    if(!fp){
        this->year = year;
        this->month = month;
        this->day = day;
        char logPath[1024]={};

        sprintf(logPath,"%s/%s.%d%02d%02d.log",logDir.c_str(),progName.c_str(),year,month,day);
        fp = fopen(logPath,"w");
        if(fp){
            logCnt += 1;
        }
    }

    else if(this->day != day){
        fclose(fp);

        this->year = year;
        this->month = month;
        this->day = day;
        char logPath[1024];
        memset(logPath,0,sizeof(logPath));
        sprintf(logPath,"%s%s. %d%02d%02d.%u.log",logDir.c_str(),progName.c_str(),year,month,day,pid);
        if(!fp){
            logCnt += 1;
        }
    }
    else if(ftell(fp) >= LOG_USE_LIMIT){
        fclose(fp);
        fclose(fp);
        char old_path[1024] = {};
        char new_path[1024] = {};
        //mv xxx.log.[i] xxx.log.[i + 1]
        for (int i = logCnt - 1;i > 0; --i)
        {
            sprintf(old_path, "%s/%s.%d%02d%02d.%u.log.%d", logDir.c_str(),progName.c_str(),year,month,day,pid, i);
            sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.%d", logDir.c_str(),progName.c_str(),year,month,day,pid, i + 1);
            rename(old_path, new_path);
        }
        //mv xxx.log xxx.log.1
        sprintf(old_path, "%s/%s.%d%02d%02d.%u.log", logDir.c_str(),progName.c_str(),year,month,day,pid);
        sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.1",logDir.c_str(),progName.c_str(),year,month,day,pid);
        rename(old_path, new_path);
        fp = fopen(old_path, "w");
        if (fp)
            logCnt += 1;
    }
    return fp != NULL;
}

void Log::append(const char* lvl,const char *format, ...){
    int ms;
    uint64_t currSecond = utcTimer.getCurrrntTime(&ms);
    if(lastErrorTime && currSecond-lastErrorTime < RELOG_TTHRESOLD)
        return;
    char logLine[LOG_LEN_LIMIT];
    int prevLen = snprintf(logLine,LOG_LEN_LIMIT,"%s[%s.%03d]",lvl,utcTimer.timeFormat.c_str(),ms);

    va_list argPtr;
    va_start(argPtr,format);

    int  mainLen = vsnprintf(logLine + prevLen,LOG_LEN_LIMIT-prevLen,format,argPtr);

    va_end(argPtr);

    uint32_t len = prevLen +mainLen;

    lastErrorTime = 0;
    bool tellBack = false;

    pthread_mutex_lock(&mutex);
    if(currBuffer->bufferStatus == false && currBuffer->availableLen()>=len){
        currBuffer->append(logLine,len);
    }
    else{
        if(currBuffer->bufferStatus == false){
            currBuffer ->bufferStatus = true;
            CellBuffer* nextBuffer = currBuffer->next;
            tellBack = true;
            if(nextBuffer -> bufferStatus == true){
                if(perBufLen*(bufCnt + 1) > MEM_USE_LIMIT){
                    fprintf(stderr,"mo more mem can use");
                    currBuffer = nextBuffer;
                    lastErrorTime = currSecond;
                }
                else{ //空间足够的话新增一个CellBuffer
                    CellBuffer* newBuffer = new CellBuffer(perBufLen);
                    bufCnt += 1;
                    newBuffer->prev = currBuffer;
                    currBuffer->next = newBuffer;
                    newBuffer->next = nextBuffer;
                    nextBuffer->prev = newBuffer;
                    currBuffer = newBuffer;
                }
            }
            else{
                currBuffer = nextBuffer;
            }
            if(!lastErrorTime){
                currBuffer->append(logLine,len);
            }
        }
        else{
            lastErrorTime = currSecond;
        }
    }
    pthread_mutex_unlock(&mutex);

    if(!tellBack)
        pthread_cond_signal(&cond);
}

void* bethdo(void *args)
{
    Log::getInstance()->persists();
    return NULL;
}