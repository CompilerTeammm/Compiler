import os
import sys
import subprocess
import filecmp
import time
import difflib
import re

compiler_path = "./build/compiler"
test_folder="./testcase/"
# python3 test.py -O1
#额外参数
extra_args = sys.argv[1:]
# pass_args=["--mem2reg"]

# grab all the testcases

# find files recursively
test_list = []
for root, dirs, files in os.walk(test_folder):
    for file in files:
        if file.endswith(".sy"):
            test_list.append(os.path.join(root, file))
CE_list = []
LLI_FAIL_list = []
WA_list = []
AC_list = []
TLE_list = []
Time_Out = []
total_time=0
Bad_test = []

for test in test_list:
    if test.endswith(".sy"):
        # add test_args to list
        print("Run pass: "+test)
        compile_args=[compiler_path, test] + extra_args #加上额外参数
        # for arg in pass_args:
        #     compile_args.append(arg)
        try:
            ret = subprocess.run(compile_args,timeout=60)
        except subprocess.TimeoutExpired:
            Time_Out.append(test)
            continue
        if ret.returncode != 0:
            CE_list.append(test)
            print("Compiler Error："+test)
            continue
        # run lli
        ll_file = test+".ll"
        out_file = test[:-2]+"out"
        # riscv file
        # asm_file = test + "..s"
        # maybe theres no input file
        
        if not os.path.exists(test[:-2]+"in") and not os.path.exists(out_file):
            Bad_test.append(test)
            continue
        
        # Kill program run over 5s, give it a TLE
        try:
            if not os.path.exists(test[:-2]+"in"):
                ret = subprocess.run(["lli-14","-opaque-pointers",ll_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
                # ret1 = subprocess.run(["riscv64-unknown-elf-as","-o","output.o",asm_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
            else:
                ret = subprocess.run(["lli-14","-opaque-pointers",ll_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, input=open(test[:-2]+"in").read().encode(), timeout=10)
                # ret1 = subprocess.run(["riscv64-unknown-elf-as","-o","output.o",asm_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
        except subprocess.TimeoutExpired:
            print("Timeout Error")
            TLE_list.append(test)
            print(test)
            total_time+=15
            continue
        #timer set
        try:
            if not os.path.exists(test[:-2] + "in"):
                ret = subprocess.run(["time", "lli-14", "-opaque-pointers", ll_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
            else:
                with open(test[:-2] + "in") as input_file:
                    input_data = input_file.read().encode()
                    ret = subprocess.run(["time", "lli-14", "-opaque-pointers", ll_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, input=input_data, timeout=10)
        
            stderr_output = ret.stderr.decode()    
            match = re.search(r'TOTAL: (\d+)H-(\d+)M-(\d+)S-(\d+)us', stderr_output)
            if match:
                hours, minutes, seconds, microseconds = map(int, match.groups())
                # 将时间转换为秒
                total_seconds = hours * 3600 + minutes * 60 + seconds + microseconds / 1e6
                # 假设total_time已经定义，并且是以秒为单位的
                total_time += total_seconds
            else:
                print("stderr_output:", stderr_output)
                print("No TOTAL time found in stderr_output.")
        except subprocess.TimeoutExpired:
            continue
        # compare the output
        # Merge The reture Value and stdout
        
        dump_str=ret.stdout.decode()
        # dump_str1 = ret1.stdout.decode()
        # remove whitesspace in the end
        # dump_str=dump_str.rstrip()
        # if dump_str1 and not dump_str1.endswith('\n'):
        #     dump_str1 += "\n"
        # if not dump_str1.endswith(''):
        #     print("RISCV Test Error")
        if dump_str and not dump_str.endswith('\n'):
            dump_str += "\n"
        dump_str += str(ret.returncode) + "\n"
        std_output=open(out_file).read()
        diff = difflib.unified_diff(dump_str.splitlines(), std_output.splitlines(), lineterm='')
        if(len(list(diff))!=0):
            print("Wrong Answer")
            WA_list.append(test)
            print(test)
            continue
        # print("Accepted")
        AC_list.append(test)
    
        
print("Compiler Error: Total: "+str(len(CE_list)))
print("Runtime Error: Total: "+str(len(LLI_FAIL_list)))
print("Timeout Error: Total: "+str(len(TLE_list)))
print("Wrong Answer: Total: "+str(len(WA_list)))
print("TimeOut Function:"+str(len(Time_Out)))
print("Bad Test: Total: "+str(len(Bad_test)))
print("Accepted: Total: "+str(len(AC_list)))
print("TOTAL TIME:"+str(total_time))

# for k in range(len(WA_list)):
#     if len(WA_list) > k:
#         print(WA_list[k])