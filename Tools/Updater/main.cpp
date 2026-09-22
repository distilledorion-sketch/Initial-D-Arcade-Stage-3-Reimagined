#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <array>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cwctype>
namespace fs=std::filesystem;
using Digest=std::array<unsigned char,32>;
struct Handle{HANDLE h=INVALID_HANDLE_VALUE;explicit Handle(HANDLE v):h(v){}~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(const Handle&)=delete;};
static void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});return s;}
static fs::path fullPath(const fs::path& p){return fs::absolute(p).lexically_normal();}
static fs::path inside(const fs::path& root,const fs::path& relative){
    auto p=fullPath(root/relative);auto prefix=lower(fullPath(root).wstring()+L"\\");
    require(lower(p.wstring()).rfind(prefix,0)==0,"An update path escaped its folder.");return p;
}
static void noLinks(fs::path p){
    // Wine exposes DOS drive mappings as reparse points. Validate every path
    // below the drive root, while allowing the drive mapping itself.
    for(p=fullPath(p);!p.empty();){if(p==p.root_path())break;DWORD a=GetFileAttributesW(p.c_str());
        if(a!=INVALID_FILE_ATTRIBUTES){
            if(a&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Update folders cannot contain links: "+p.u8string());
            Handle check(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
            require(check.h!=INVALID_HANDLE_VALUE,"Cannot inspect final update path.");
            std::vector<wchar_t> resolved(32768);DWORD length=GetFinalPathNameByHandleW(check.h,resolved.data(),static_cast<DWORD>(resolved.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
            require(length>0&&length<resolved.size(),"Cannot resolve final update path.");
            std::wstring final(resolved.data(),length);if(final.rfind(L"\\\\?\\",0)==0)final=final.substr(4);
            require(lower(fullPath(final).wstring())==lower(p.wstring()),"An update path resolves through a link.");
        }
        else require(GetLastError()==ERROR_FILE_NOT_FOUND||GetLastError()==ERROR_PATH_NOT_FOUND,"Cannot inspect an update path.");
        auto parent=p.parent_path();if(parent==p)break;p=parent;
    }
}
static Digest hash(const fs::path& path){
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    require(file.h!=INVALID_HANDLE_VALUE,"Cannot read a game file.");
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    require(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 unavailable.");
    Digest result{};std::vector<unsigned char> buffer(256*1024);
    try{
        require(BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)>=0,"Cannot create SHA256 hash.");
        DWORD size=0;while(true){require(ReadFile(file.h,buffer.data(),static_cast<DWORD>(buffer.size()),&size,nullptr)!=0,"Cannot read game data.");if(!size)break;require(BCryptHashData(h,buffer.data(),size,0)>=0,"Cannot hash game data.");}
        require(BCryptFinishHash(h,result.data(),32,0)>=0,"Cannot finish SHA256 hash.");
    }catch(...){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);throw;}
    BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);return result;
}
static bool oneOf(const std::wstring& s,std::initializer_list<const wchar_t*> values){for(auto v:values)if(s==lower(v))return true;return false;}
static fs::path safeName(std::wstring s){
    require(!s.empty()&&s.size()<=220,"Invalid update filename.");std::replace(s.begin(),s.end(),L'/',L'\\');
    size_t from=0;std::vector<std::wstring> parts;
    while(true){auto at=s.find(L'\\',from);auto p=s.substr(from,at==s.npos?s.npos:at-from);auto l=lower(p);
        require(!p.empty()&&p!=L"."&&p!=L".."&&p.back()!=L'.'&&p.back()!=L' ',"Unsafe update filename.");
        for(auto c:p)require(c>=32&&std::wstring(L"<>:\"|?*").find(c)==std::wstring::npos,"Invalid filename character.");
        auto stem=l.substr(0,l.find(L'.'));
        require(!oneOf(stem,{L"con",L"prn",L"aux",L"nul"})&&!(stem.size()==4&&(stem.substr(0,3)==L"com"||stem.substr(0,3)==L"lpt")&&stem[3]>=L'1'&&stem[3]<=L'9'),"Reserved filename.");
        require(!oneOf(l,{L"userdata",L"userdata-unity-scene",L"community-times",L"replays",L"custom-music",L"admin-access.txt",L"identity.json",L"game-options.json",L"deploy.private.json",L"library.json",L"pending.json"}),"Personal files cannot be updated.");
        parts.push_back(l);if(at==s.npos)break;from=at+1;
    }
    require(parts.size()>1?oneOf(parts[0],{L"initialdunity_data",L"monobleedingedge",L"d3d12"}):oneOf(parts[0],{L"initialdunity.exe",L"unityplayer.dll",L"unitycrashhandler64.exe",L"dstorage.dll",L"dstoragecore.dll",L"steam_appid.txt",L"read me.txt",L"replay viewer.cmd",L"multiplayer test.txt"}),"Unexpected game path.");
    return fs::path(s);
}
static void write(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<text;require(bool(f),"Cannot write update status.");}
template<class T>static T read(std::ifstream& f){T value{};f.read(reinterpret_cast<char*>(&value),sizeof value);require(bool(f),"Truncated install plan.");return value;}
static std::wstring readString(std::ifstream& f){auto n=read<uint32_t>(f);require(n>0&&n<32768,"Invalid plan string.");std::wstring value(n,L'\0');f.read(reinterpret_cast<char*>(value.data()),static_cast<std::streamsize>(n*2));require(bool(f)&&value.find(L'\0')==value.npos,"Invalid plan string.");return value;}
struct Record{fs::path relative,dest,stage,backup;bool changed=false,existed=false;uint64_t size=0;Digest old{},next{};};
static void verifyOld(const Record& r){noLinks(r.dest);require(fs::exists(r.dest)==r.existed,"Game files changed during update preparation.");if(r.existed)require(hash(r.dest)==r.old,"Game files changed during update preparation.");}
static uint64_t creation(HANDLE h){FILETIME a{},b{},c{},d{};require(GetProcessTimes(h,&a,&b,&c,&d)!=0,"Cannot identify the game process.");return (static_cast<uint64_t>(a.dwHighDateTime)<<32)|a.dwLowDateTime;}
static void noOtherGame(const fs::path& game,DWORD parent){
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));require(snapshot.h!=INVALID_HANDLE_VALUE,"Cannot check running games.");PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
    if(Process32FirstW(snapshot.h,&entry))do{if(entry.th32ProcessID==parent||_wcsicmp(entry.szExeFile,L"InitialDUnity.exe"))continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID));if(!process.h)continue;
        wchar_t path[32768];DWORD length=32768;if(QueryFullProcessImageNameW(process.h,0,path,&length))require(lower(fullPath(path).wstring())!=lower(fullPath(game).wstring()),"Another copy of this game is running.");
    }while(Process32NextW(snapshot.h,&entry));
}
static void restart(const fs::path& root){
    auto exe=root/L"InitialDUnity.exe";std::wstring command=L"\""+exe.wstring()+L"\" -idas3-skip-update-once";
    STARTUPINFOW start{};start.cb=sizeof start;PROCESS_INFORMATION p{};
    require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&start,&p)!=0,"Update installed, but restarting the game failed. Launch it normally.");CloseHandle(p.hThread);CloseHandle(p.hProcess);
}
static void removeTree(const fs::path& session,const wchar_t* name){auto p=inside(session,name);noLinks(p);if(!fs::exists(p))return;for(const auto& entry:fs::recursive_directory_iterator(p))noLinks(entry.path());fs::remove_all(p);}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);fs::path session,root;std::vector<Record> records;std::vector<size_t> applied;bool ready=false,committed=false,test=false;
    try{
        require(argv&&(argc==2||argc==3),"Invalid installer arguments.");test=argc==3&&std::wstring(argv[2])==L"--test";require(argc!=3||test,"Invalid test option.");
        auto plan=fullPath(argv[1]);session=plan.parent_path();require(plan.filename()==L"install.plan","Invalid install plan filename.");noLinks(session);noLinks(plan);require(fs::file_size(plan)<=64*1024*1024,"Install plan too large.");
        std::ifstream input(plan,std::ios::binary);char magic[8];input.read(magic,8);require(std::string(magic,8)=="IDUPD002","Invalid install plan.");
        root=fullPath(readString(input));noLinks(root);auto game=root/L"InitialDUnity.exe";require(fs::is_regular_file(game),"Game executable missing.");
        require(lower(session.wstring()).rfind(lower(root.wstring()+L"\\"),0)!=0&&root!=session,"Installer must be outside the game folder.");
        auto pid=read<uint32_t>(input);auto stamp=read<uint64_t>(input);auto count=read<uint32_t>(input);require(count>0&&count<=100000,"Invalid file count.");
        if(test)require(fs::is_regular_file(root.parent_path()/L"ISOLATED_UPDATE_TEST.txt"),"Tests require an isolated fixture.");else require(pid>0,"Game process missing.");
        Handle parent(pid?OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid):nullptr);
        if(pid){require(parent.h!=nullptr,"Game process unavailable.");require(creation(parent.h)==stamp,"Game process identity changed.");}
        auto lock=root/L".update-lock";noLinks(lock);Handle lockFile(CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE,nullptr));require(lockFile.h!=INVALID_HANDLE_VALUE,"Another update is running.");
        std::vector<std::wstring> names;uint64_t total=0;
        for(uint32_t i=0;i<count;++i){Record r;r.relative=safeName(readString(input));auto changed=read<uint8_t>(input),existed=read<uint8_t>(input);require(changed<=1&&existed<=1,"Invalid file flags.");r.changed=changed!=0;r.existed=existed!=0;r.size=read<uint64_t>(input);r.old=read<Digest>(input);r.next=read<Digest>(input);require(r.size<=24ULL*1024*1024*1024,"File too large.");total+=r.size;require(total<=24ULL*1024*1024*1024,"Update too large.");
            require(r.changed||(r.existed&&r.old==r.next),"Invalid retained file.");r.dest=inside(root,r.relative);r.stage=inside(session,fs::path(L"stage")/r.relative);r.backup=inside(session,fs::path(L"backup")/r.relative);names.push_back(lower(r.relative.wstring()));records.push_back(r);
        }
        require(input.peek()==std::char_traits<char>::eof(),"Trailing plan data.");std::sort(names.begin(),names.end());require(std::adjacent_find(names.begin(),names.end())==names.end(),"Duplicate game path.");
        for(auto name:{L"initialdunity.exe",L"unityplayer.dll",L"initialdunity_data\\globalgamemanagers",L"initialdunity_data\\managed\\assembly-csharp.dll"})require(std::binary_search(names.begin(),names.end(),name),"Incomplete game inventory.");
        if(!test)noOtherGame(game,pid);
        for(const auto& r:records){verifyOld(r);if(!r.changed)continue;noLinks(r.stage);noLinks(r.backup);require(fs::file_size(r.stage)==r.size&&hash(r.stage)==r.next,"Staged game file failed verification.");if(r.existed){fs::create_directories(r.backup.parent_path());require(CopyFileW(r.dest.c_str(),r.backup.c_str(),TRUE)!=0,"Cannot back up game file.");require(hash(r.backup)==r.old,"Backup verification failed.");}}
        write(session/L"ready","Prepared");ready=true;
        if(parent.h)require(WaitForSingleObject(parent.h,5*60*1000)==WAIT_OBJECT_0,"The game did not close; no files were replaced.");
        if(!test)noOtherGame(game,0);
        for(const auto& r:records)verifyOld(r);
        for(size_t i=0;i<records.size();++i){auto& r=records[i];if(!r.changed)continue;noLinks(r.dest);noLinks(r.stage);require(hash(r.stage)==r.next,"Staged file changed.");fs::create_directories(r.dest.parent_path());applied.push_back(i);require(CopyFileW(r.stage.c_str(),r.dest.c_str(),FALSE)!=0,"Cannot replace a game file.");require(hash(r.dest)==r.next,"Installed file failed verification.");}
        committed=true;write(session/L"result.json","{\"passed\":true,\"changedFiles\":"+std::to_string(applied.size())+"}");
        if(!test)restart(root);
        // Clean only checked, uniquely-created session paths. Never remove game files here.
        try{removeTree(session,L"stage");removeTree(session,L"backup");auto archive=inside(session,L"game.zip");noLinks(archive);fs::remove(archive);}catch(...){/* Safe to leave temporary data after a successful update. */}
        LocalFree(argv);return 0;
    }catch(const std::exception& e){
        std::string error=e.what();if(!committed)for(auto it=applied.rbegin();it!=applied.rend();++it){auto& r=records[*it];try{noLinks(r.dest);if(r.existed){noLinks(r.backup);require(hash(r.backup)==r.old,"Invalid rollback backup.");if(!fs::exists(r.dest)||hash(r.dest)!=r.old)require(CopyFileW(r.backup.c_str(),r.dest.c_str(),FALSE)!=0,"Cannot restore game file.");require(hash(r.dest)==r.old,"Rollback hash mismatch.");}else fs::remove(r.dest);}catch(const std::exception& restore){error+=" Rollback needs attention: ";error+=restore.what();}}
        if(!session.empty())try{write(session/L"error.txt",error);}catch(...){}
        if(ready&&!test){MessageBoxA(nullptr,error.c_str(),"Initial D update",MB_OK|MB_ICONERROR);if(!committed)try{restart(root);}catch(...){}}
        if(argv)LocalFree(argv);return 1;
    }
}
