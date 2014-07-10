/**
 * 使用libcurl库
 * 编译:
 * gcc -o main download.c -lcurl
 * ./main --get "http://www.baidu.com"  //get请求
 * ./main --post "name=sielnceper&age=22" "http://www.baidu.com" //post请求并发送数据
 *
 * tips:命令的选项做的不好  现在只是测一下curl   可参考《UNIX环境高级编程》 p621 getopt函数
 */
#include <stdio.h>
#include <string.h>
#include "download.h"
#define OPT_GET "--get"
#define OPT_POST "--post"
int main(char argc,char *argv[]){
    if(argc<3){
        fprintf(stderr,"参数错误!\n");
        return -1;  
    }
    DownloadContent content;
    content.text=NULL;
    content.size=0;
    int code=0;
    
    if(!strcmp(OPT_GET,argv[1])){
        //get获取数据
        code=get_content(argv[2],&content);
    }else if(!strcmp(OPT_POST,argv[1])){
        //post获取数据
        code=post_content(argv[3],argv[2],&content);
    }else{
        fprintf(stderr,"未知的选项\n");
        return -1;
    }
    if(code!=0){
        free(content.text);    
        fprintf(stderr,"请求失败\n");
        return -1;
    }

    printf("content=%s\n",content.text);
    free(content.text);    
    
    return 0;
}
