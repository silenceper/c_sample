#include "say.h"
char* say(char *name,int age){
    char *str=NULL;
    str=malloc(sizeof(char*)*strlen(name)+1);
    sprintf(str,"my name is %s,my age is %d",name,age);
    return str;
}
