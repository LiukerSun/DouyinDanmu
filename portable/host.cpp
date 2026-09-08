#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <iostream>

// A Windows job owns the supervisor and all descendants. Closing the console
// cannot leave a collector or database process running invisibly.
static std::wstring quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){slashes++;continue;}
        out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');return out+L"\"";
}
int wmain(int argc,wchar_t** argv) {
    wchar_t filename[32768];DWORD length=GetModuleFileNameW(nullptr,filename,32768);
    if(!length || length>=32768)return 1;
    std::wstring executable(filename,length),root=executable.substr(0,executable.find_last_of(L"\\/"));
    std::wstring node=root+L"\\runtime\\node.exe", command=quote(node)+L" "+quote(root+L"\\app\\portable\\launcher.js");
    for(int i=1;i<argc;i++)command+=L" "+quote(argv[i]);
    HANDLE job=CreateJobObjectW(nullptr,nullptr);if(!job)return 1;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){CloseHandle(job);return 1;}
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
    if(!CreateProcessW(node.c_str(),buffer.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,root.c_str(),&startup,&process)) {
        std::cerr<<"Cannot start bundled runtime: "<<GetLastError()<<std::endl;CloseHandle(job);return 1;
    }
    if(!AssignProcessToJobObject(job,process.hProcess)){TerminateProcess(process.hProcess,1);CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(job);return 1;}
    // Node handles Ctrl+C and performs orderly shutdown; other console close
    // events use the OS job's process-tree cleanup.
    SetConsoleCtrlHandler([](DWORD signal)->BOOL{return signal==CTRL_C_EVENT || signal==CTRL_BREAK_EVENT;},TRUE);
    ResumeThread(process.hThread);CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);
    CloseHandle(process.hProcess);CloseHandle(job);return static_cast<int>(code);
}
