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

// #define OPT
// #define backend

// extern FILE *yyin;
// extern int optind, opterr, optopt;
// extern char *optargi;
// std::string asmoutput_path;
// void copyFile(const std::string &sourcePath, const std::string &destinationPath) {
//   std::ifstream source(sourcePath, std::ios::binary);
//   std::ofstream destination(destinationPath, std::ios::binary);
// //   destination << source.rdbuf();
// }

// // hu1 add it:解析 --test=xxx,yyy
// std::vector<std::string> parseTestArg(const std::string& arg) {
//    std::vector<std::string> result;
//    size_t start = 0;
//    while (start < arg.size()) {
//        size_t end = arg.find(',', start);
//        if (end == std::string::npos) end = arg.size();
//        result.push_back(arg.substr(start, end - start));
//        start = end + 1;
//    }
//    return result;
// }

// int main(int argc, char **argv) {
//   std::string output_path = argv[1];
//   output_path += ".ll";

//   std::string filename = argv[1];
//   size_t lastSlashPos = filename.find_last_of("/\\") + 1;
//   filename = filename.substr(lastSlashPos);

//   asmoutput_path = argv[1];
//   size_t lastPointPos = asmoutput_path.find_last_of(".");
//   asmoutput_path = asmoutput_path.substr(0, lastPointPos) + ".s";

//   copyFile("runtime.ll", output_path);
//   freopen(output_path.c_str(), "a", stdout);
//   yyin = fopen(argv[1], "r");
//   yy::parser parse;
//   parse();
//   Singleton<CompUnit *>()->codegen();

//    // 中端 前端要是测试可以把这段代码注释掉即可
// #ifdef OPT
//    // auto PM = std::make_unique<PassManager>();
//    // PM->RunOnTest();

//    PassManager PM;
//    bool optimization_set = false;
   
//    for (int i = 1; i < argc; ++i) {
//        std::string arg = argv[i];
   
//        // 识别 -O1
//        if (arg == "-O1") {
//            PM.SetLevel(O1);
//            optimization_set = true;
//        }
   
//        // 识别 --test=xxx,yyy
//        else if (arg.rfind("--test=", 0) == 0) {
//            std::string param = arg.substr(7); // 去掉前缀 "--test="
//            auto tags = parseTestArg(param);   // 你原来的 parseTestArg 可复用
//            PM.EnableTestPasses(tags);
//            optimization_set = true;
//        }
//    }
   
//    if (!optimization_set) {
//        PM.SetLevel(None); // 默认只开 mem2reg
//    }
   
//    PM.Run();
// #endif

//    Singleton<Module>().Test();
//    fflush(stdout);
//    fclose(stdout);

// #ifdef backend
//    // 后端
//    freopen(asmoutput_path.c_str(), "w", stdout);
//    TransModule RISCVAsm;
//    RISCVAsm.run(&Singleton<Module>());

//    fflush(stdout);
//    fclose(stdout);
// #endif

//    return 0;
// }

// #include "./include/lib/CoreClass.hpp"
// #include "./yacc/parser.hpp"
// #include <fstream>
// #include <getopt.h>
// #include <iostream>
// #include <memory>
// #include "include/IR/Opt/PassManager.hpp"
// #include "include/MyBackend/MIR.hpp"
// #include "include/MyBackend/Translate.hpp"
// #include "Log/log.hpp"
// #include <filesystem>

// 输入指令是 compiler -S -o test.s test.sy

extern FILE *yyin;
std::string asmoutput_path;
void copyFile(const std::string &sourcePath, const std::string &destinationPath) {
  std::ifstream source(sourcePath, std::ios::binary);
  std::ofstream destination(destinationPath, std::ios::binary);
  destination << source.rdbuf();
}

// hu1 add it:解析 --test=xxx,yyy
std::vector<std::string> parseTestArg(const std::string& arg) {
   std::vector<std::string> result;
   size_t start = 0;
   while (start < arg.size()) {
       size_t end = arg.find(',', start);
       if (end == std::string::npos) end = arg.size();
       result.push_back(arg.substr(start, end - start));
       start = end + 1;
   }
   return result;
}

int main(int argc, char **argv) {
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

   // 中端 前端要是测试可以把这段代码注释掉即可
#ifdef OPT
   // auto PM = std::make_unique<PassManager>();
   // PM->RunOnTest();

   PassManager PM;
   bool optimization_set = false;
   
   for (int i = 1; i < argc; ++i) {
       std::string arg = argv[i];
   
       // 识别 -O1
       if (arg == "-O1") {
           PM.SetLevel(O1);
           optimization_set = true;
       }
       //识别hu1的test(非线性)
       else if(arg=="hu1"){
         PM.SetLevel(hu1_test);
         optimization_set = true;
       }
       // 识别 --test=xxx,yyy
       else if (arg.rfind("--test=", 0) == 0) {
           std::string param = arg.substr(7); // 去掉前缀 "--test="
           auto tags = parseTestArg(param);   // 你原来的 parseTestArg 可复用
           PM.EnableTestPasses(tags);
           optimization_set = true;
       }
   }
   
   if (!optimization_set) {
       PM.SetLevel(None); // 默认只开 mem2reg
   }
   
   PM.Run();
   
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