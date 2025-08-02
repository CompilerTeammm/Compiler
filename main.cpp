#include "./include/lib/CoreClass.hpp"
#include "./yacc/parser_output.hpp"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <memory>
#include "include/IR/Opt/PassManager.hpp"
#include "include/MyBackend/MIR.hpp"
#include "include/MyBackend/Translate.hpp"
#include "Log/log.hpp"
#include <filesystem>


// 输入指令是 compiler -S -o test.s test.sy

extern FILE *yyin;
std::string asmoutput_path;
int main(int argc, char **argv){
   
   asmoutput_path = argv[3];
   size_t lastPointPos = asmoutput_path.find_last_of(".");
   asmoutput_path = asmoutput_path.substr(0, lastPointPos) + ".s";

   yyin = fopen(argv[4], "r");
   yy::parser parse;
   parse();
   Singleton<CompUnit *>()->codegen();

   freopen(asmoutput_path.c_str(), "w", stdout);
   TransModule RISCVAsm;
   RISCVAsm.run(&Singleton<Module>());

   fflush(stdout);
   fclose(stdout);

   return 0;
}