#include <iostream>
#include <string.h>
#include <errno.h>
using std::cout;
using std::endl;
void error_detection(int function_flag,const char *error_source)
{
    if(function_flag<0)
    {
        cout<<error_source<<": an error occured, error information: "<<strerror(errno)<<endl;
        exit(-1);
    }
    
}
