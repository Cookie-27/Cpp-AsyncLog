#include<../log/log.h>


int main(){
    LOG_INIT("logFile","progname",1);
    for(int i=0;i<100;i++){
        ERROR("test log number is %d", i);
    }
    return 0;
}

