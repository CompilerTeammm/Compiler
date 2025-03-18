///// for test
// #include <memory>
// #include <bits/unique_ptr.h>
// #include "./include/IR/Opt/PassManager.hpp"
// #include "MemoryToRegister.hpp"

// class Mem2Reg;
// int main()
// {
//    auto func = Get();
//    auto passManager = std::make_unique<PassManager>();
//    passManager->addPass(mem2reg);
//    passManager->RunImpl<Mem2Reg, Function>(func);
// }

#include "./include/lib/CoreClass.hpp"
// #include "../include/ir/opt/New_passManager.hpp"
#include "./yacc/parser.hpp"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <memory>

extern FILE *yyin;
extern int optind, opterr, optopt;
extern char *optargi;
std::string asmoutput_path;
void copyFile(const std::string &sourcePath,
              const std::string &destinationPath)
{
   std::ifstream source(sourcePath, std::ios::binary);
   std::ofstream destination(destinationPath, std::ios::binary);
   destination << source.rdbuf();
}

int main(int argc, char **argv)
{
   std::string output_path = argv[1];
   output_path += ".ll";

   std::string filename = argv[1];
   size_t lastSlashPos = filename.find_last_of("/\\") + 1;
   filename = filename.substr(lastSlashPos);

   asmoutput_path = argv[1];
   size_t lastPointPos = asmoutput_path.find_last_of(".");
   asmoutput_path = asmoutput_path.substr(0, lastPointPos) + ".s";

   copyFile("runtime.ll", output_path);
   freopen(output_path.c_str(), "a", stdout);
   yyin = fopen(argv[1], "r");
   yy::parser parse;
   parse();
   Singleton<CompUnit *>()->codegen();
   // auto PM = std::make_unique<_PassManager>();
   // PM->DecodeArgs(argc, argv);
   // PM->RunOnTest();
   Singleton<Module>().Test();
   fflush(stdout);
   fclose(stdout);
   return 0;
}