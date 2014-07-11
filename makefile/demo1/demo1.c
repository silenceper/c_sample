#include "demo1.h"
int main(int argc,char *argv[]){
    char *str=say("wenzhenlin",22);
    printf(str);
    free(str);
    return 0;
}
