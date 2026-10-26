#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <iostream>

// This bootstrap stays in the install root; data/logs never move with versions.
static std::wstring quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){slashes++;continue;}
        out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');return out+L"\"";
}
static bool regular_path(const std::wstring& root,const std::wstring& relative,bool directory=false) {
    auto current=root;size_t start=0;
    while(start<relative.size()) {
        const auto end=relative.find(L'\\',start);
        const auto component=relative.substr(start,end==std::wstring::npos?end:end-start);
        if(component.empty() || component==L"." || component==L"..")return false;
        current+=L"\\"+component;
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        const bool is_directory=(attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        if(end==std::wstring::npos)return is_directory==directory;
        if(!is_directory)return false;
        start=end+1;
    }
    return false;
}
static bool exists(const std::wstring& path) { return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES; }
static bool read_version_id(const std::wstring& root,const std::wstring& name,std::wstring& id) {
    if(!regular_path(root,L".updates\\"+name))return false;
    HANDLE file=CreateFileW((root+L"\\.updates\\"+name).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    char bytes[65]{};DWORD count=0;const bool read=ReadFile(file,bytes,sizeof(bytes),&count,nullptr)!=FALSE;CloseHandle(file);
    if(!read || count<32 || count>34)return false;
    for(DWORD i=0;i<32;i++)if(!((bytes[i]>='0'&&bytes[i]<='9')||(bytes[i]>='a'&&bytes[i]<='f')))return false;
    for(DWORD i=32;i<count;i++)if(bytes[i]!='\r'&&bytes[i]!='\n')return false;
    id.assign(bytes,bytes+32);return true;
}
static bool version_ready(const std::wstring& root,const std::wstring& id) {
    const auto relative=L".versions\\"+id;
    if(!regular_path(root,relative,true))return false;
    for(const auto* file:{L"runtime\\node.exe",L"app\\portable\\launcher.js",L"bin\\douyin-pipeline.exe",L"web\\index.html",L"VERSION",L"update.json"})
        if(!regular_path(root,relative+L"\\"+file))return false;
    return true;
}
static bool activate_pending(const std::wstring& root) {
    std::wstring id;
    if(!read_version_id(root,L"pending-version",id) || !version_ready(root,id))return false;
    // Atomic pointer replacement leaves the previous active program intact on failure.
    return MoveFileExW((root+L"\\.updates\\pending-version").c_str(),(root+L"\\.updates\\active-version").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
static bool stop_job(HANDLE job) {
    if(!TerminateJobObject(job,0))return false;
    for(int attempt=0;attempt<1000;++attempt) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        if(!QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr))return false;
        if(accounting.ActiveProcesses==0)return true;
        Sleep(10);
    }
    return false;
}
int wmain(int argc,wchar_t** argv) {
    wchar_t filename[32768];DWORD length=GetModuleFileNameW(nullptr,filename,32768);
    if(!length || length>=32768)return 1;
    const std::wstring executable(filename,length),root=executable.substr(0,executable.find_last_of(L"\\/"));
    const auto attributes=GetFileAttributesW(root.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return 1;
    if(!SetEnvironmentVariableW(L"DOUYIN_INSTALL_ROOT",root.c_str()) || !SetEnvironmentVariableW(L"DOUYIN_UPDATE_PROTOCOL",L"1"))return 1;
    SetConsoleCtrlHandler([](DWORD signal)->BOOL{return signal==CTRL_C_EVENT || signal==CTRL_BREAK_EVENT;},TRUE);
    while(true) {
        auto program=root;
        if(exists(root+L"\\.updates\\active-version")) {
            std::wstring id;
            if(!read_version_id(root,L"active-version",id) || !version_ready(root,id)) {
                std::cerr<<"The active update is incomplete. Program files were not rolled back; user data is preserved."<<std::endl;return 1;
            }
            program=root+L"\\.versions\\"+id;
        }
        const auto node=program+L"\\runtime\\node.exe";
        auto command=quote(node)+L" "+quote(program+L"\\app\\portable\\launcher.js");
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
        ResumeThread(process.hThread);CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
        const bool stopped=code!=75 || stop_job(job);
        CloseHandle(job);
        if(code!=75)return static_cast<int>(code);
        if(!stopped || !activate_pending(root)) {
            std::cerr<<"Update activation failed. The previous program and all user data are preserved."<<std::endl;return 1;
        }
        std::cout<<"Update verified; restarting the new version."<<std::endl;
    }
}
