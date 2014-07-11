#include "say.h"
char* say(char *name,int age){
    char *str=NULL;
    char *format="my name is %s,my age is %d";
    size_t length=strlen(format)+strlen(name)+sizeof(int)+1-2-2; //是这样的吗
    str=(char *)malloc(length);
    sprintf(str,"my name is %s,my age is %d",name,age);
    return str;
}
