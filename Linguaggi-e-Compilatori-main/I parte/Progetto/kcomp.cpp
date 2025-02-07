#include <iostream>
#include "driver.hpp"

int main(int argc, char* argv[]){
    Driver driver;
    for(int i=1; i<argc; i++){
        if(argv[i]==std::string("-p") ){
            driver.trace_parsing=true;
        }else if(argv[i]==std::string("-s")){
            driver.trace_scanning=true;
        }else if(!driver.parse(argv[i])){
            driver.codegen();
        }else{
            return 1;
        }
    }
    return 0;
}