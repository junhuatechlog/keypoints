#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <list>
using namespace std;

int main(int argc, char * argv[]){
    std::list<std::string> lines;
    fstream stream;
    std::string filename="/tmp/hwapi/hwapitest.log";
    char *details="This is a test program!\n";
    int value = 0x12345678;

    if(argc != 3){
        printf("./streamtest filename blocked!\n"); 
        return 0;
    }
    stream.open(argv[1], std::ios_base::in);
    if(!stream.is_open()){
        cout<<"Can't open "<<filename<<endl;
        //return -1;
    }

    while(stream.good()){
        std::string fieldValue;
        std::getline(stream, fieldValue);
        if(lines.size() < 10)
            lines.push_back(fieldValue);
    }
    stream.close();

    stream.open(argv[1], std::ios_base::out |std::ios_base::trunc);
    if(!stream.is_open()){
        cout<<"Can't open "<<filename<<"for write!"<<endl;

        return -2;
    }

    char hexValue[100] = {0};
    snprintf(hexValue, 99, "0x%X :%s", value, details );
    stream << hexValue;
    if(atoi(argv[2]) == 1){
        cout<<"block before close!"<<endl;
        while(1);
    }

    while (!lines.empty())
    {
        stream << '\n';
        stream << lines.front();
        lines.pop_front();
    }
    stream.flush();
    stream.close();

    return 0;
}
