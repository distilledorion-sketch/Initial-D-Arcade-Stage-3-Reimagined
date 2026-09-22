#include <windows.h>
#include <string>
#include <fstream>
#include <filesystem>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR args,int){
    if(std::wstring(args)==L"-wait"){
        FILETIME start{},a{},b{},c{};GetProcessTimes(GetCurrentProcess(),&start,&a,&b,&c);
        std::ofstream file("parent.txt");file<<GetCurrentProcessId()<<" "<<((static_cast<unsigned long long>(start.dwHighDateTime)<<32)|start.dwLowDateTime);file.close();
        for(int i=0;i<1200&&!std::filesystem::exists("release-parent");i++)Sleep(100);
    }else{std::ofstream("restarted.txt")<<"restarted";}
    return 0;
}
