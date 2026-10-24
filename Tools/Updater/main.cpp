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
#include <regex>
namespace fs=std::filesystem;
using Digest=std::array<unsigned char,32>;
struct Handle{HANDLE h=INVALID_HANDLE_VALUE;explicit Handle(HANDLE v):h(v){}~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(const Handle&)=delete;};
static void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});return s;}
static fs::path fullPath(const fs::path& p){return fs::absolute(p).lexically_normal();}
static fs::path comparablePath(std::wstring value){
    auto prefix=lower(value);
    if(prefix.rfind(L"\\\\?\\unc\\",0)==0)value=L"\\\\"+value.substr(8);
    else if(prefix.rfind(L"\\\\?\\",0)==0)value=value.substr(4);
    return fullPath(value);
}
static fs::path inside(const fs::path& root,const fs::path& relative){
    auto p=fullPath(root/relative);auto prefix=lower(fullPath(root).wstring()+L"\\");
    require(lower(p.wstring()).rfind(prefix,0)==0,"An update path escaped its folder.");return p;
}
static fs::path noReparseAncestors(fs::path p){
    // Wine exposes DOS drive mappings as reparse points. Validate every path
    // below the drive root, while allowing the drive mapping itself.
    fs::path existing;
    for(p=fullPath(p);!p.empty();){if(p==p.root_path())break;DWORD a=GetFileAttributesW(p.c_str());
        if(a!=INVALID_FILE_ATTRIBUTES){
            if(a&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Update folders cannot contain links: "+p.u8string());
            if(existing.empty())existing=p;
        }
        else require(GetLastError()==ERROR_FILE_NOT_FOUND||GetLastError()==ERROR_PATH_NOT_FOUND,"Cannot inspect an update path.");
        auto parent=p.parent_path();if(parent==p)break;p=parent;
    }
    return existing;
}
static bool sameWineDriveAlias(const fs::path& expected,const fs::path& resolved,HANDLE original){
    // Wine may return C:\\... for the exact same directory opened via Z:\\... .
    // This is a DOS drive alias, not a link inside the update tree. Retain
    // link rejection on BOTH spellings and require matching open-file identity.
    if(!GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"wine_get_version")||
        lower(expected.root_path().wstring())==lower(resolved.root_path().wstring()))return false;
    noReparseAncestors(resolved);
    Handle other(CreateFileW(resolved.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION a{},b{};
    return other.h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(original,&a)&&GetFileInformationByHandle(other.h,&b)&&
        !(b.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&
        a.nFileIndexHigh==b.nFileIndexHigh&&a.nFileIndexLow==b.nFileIndexLow;
}
static fs::path longPath(const fs::path& p){
    // Expand DOS 8.3 names without resolving junctions or symbolic links.
    // TEMP and user profiles can use short names even for ordinary folders.
    std::vector<wchar_t> expanded(32768);
    DWORD length=GetLongPathNameW(p.c_str(),expanded.data(),static_cast<DWORD>(expanded.size()));
    if(length>0&&length<expanded.size())return comparablePath(std::wstring(expanded.data(),length));
    // Wine's long-name expansion can fail on a Z: spelling inside drive_c
    // even though CreateFileW succeeds. Only accept the verified drive alias.
    if(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"wine_get_version")){
        noReparseAncestors(p);
        Handle file(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        length=GetFinalPathNameByHandleW(file.h,expanded.data(),static_cast<DWORD>(expanded.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
        if(length>0&&length<expanded.size()){
            auto resolved=comparablePath(std::wstring(expanded.data(),length));
            if(sameWineDriveAlias(fullPath(p),resolved,file.h))return resolved;
        }
    }
    throw std::runtime_error("Cannot expand an update path.");
}
static void noLinks(fs::path p){
    auto existing=noReparseAncestors(p);
    // Resolving the closest existing path resolves all of its ancestors too.
    // Do not reopen and resolve the same directories for every ancestor/file.
    if(!existing.empty()){
        Handle check(CreateFileW(existing.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
        require(check.h!=INVALID_HANDLE_VALUE,"Cannot inspect final update path.");
        std::vector<wchar_t> resolved(32768);DWORD length=GetFinalPathNameByHandleW(check.h,resolved.data(),static_cast<DWORD>(resolved.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
        require(length>0&&length<resolved.size(),"Cannot resolve final update path.");
        auto actual=comparablePath(std::wstring(resolved.data(),length));auto expected=longPath(existing);
        require(lower(actual.wstring())==lower(expected.wstring())||sameWineDriveAlias(expected,actual,check.h),"An update path resolves through a link.");
    }
}
static fs::path canonicalExistingPath(const fs::path& p){
    noLinks(p);
    Handle file(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
    std::vector<wchar_t> resolved(32768);
    DWORD length=GetFinalPathNameByHandleW(file.h,resolved.data(),static_cast<DWORD>(resolved.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    require(length>0&&length<resolved.size(),"Cannot resolve final update path.");
    // Use one spelling for containment/process checks too, not just validation.
    return comparablePath(std::wstring(resolved.data(),length));
}
struct HashWorkspace{
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    std::vector<unsigned char> buffer=std::vector<unsigned char>(256*1024);
    HashWorkspace(){require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 unavailable.");}
    ~HashWorkspace(){BCryptCloseAlgorithmProvider(algorithm,0);}
};
static Digest hash(const fs::path& path){
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    require(file.h!=INVALID_HANDLE_VALUE,"Cannot read a game file.");
    static HashWorkspace workspace;
    BCRYPT_HASH_HANDLE h=nullptr;Digest result{};auto& buffer=workspace.buffer;
    try{
        require(BCryptCreateHash(workspace.algorithm,&h,nullptr,0,nullptr,0,0)>=0,"Cannot create SHA256 hash.");
        DWORD size=0;while(true){require(ReadFile(file.h,buffer.data(),static_cast<DWORD>(buffer.size()),&size,nullptr)!=0,"Cannot read game data.");if(!size)break;require(BCryptHashData(h,buffer.data(),size,0)>=0,"Cannot hash game data.");}
        require(BCryptFinishHash(h,result.data(),32,0)>=0,"Cannot finish SHA256 hash.");
    }catch(...){if(h)BCryptDestroyHash(h);throw;}
    BCryptDestroyHash(h);return result;
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
static void write(const fs::path& p,const std::string& text){noLinks(p);std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<text;f.close();require(bool(f),"Cannot write update status.");}
template<class T>static T read(std::ifstream& f){T value{};f.read(reinterpret_cast<char*>(&value),sizeof value);require(bool(f),"Truncated install plan.");return value;}
static std::wstring readString(std::ifstream& f){auto n=read<uint32_t>(f);require(n>0&&n<32768,"Invalid plan string.");std::wstring value(n,L'\0');f.read(reinterpret_cast<char*>(value.data()),static_cast<std::streamsize>(n*2));require(bool(f)&&value.find(L'\0')==value.npos,"Invalid plan string.");return value;}
struct Record{fs::path relative,dest,stage,backup;bool changed=false,existed=false;uint64_t size=0;Digest old{},next{};};
static std::vector<Record> readRecords(std::ifstream& input,const fs::path& root,const fs::path& session){
    auto count=read<uint32_t>(input);require(count>0&&count<=100000,"Invalid file count.");
    std::vector<Record> records;std::vector<std::wstring> names;uint64_t total=0;
    for(uint32_t i=0;i<count;++i){Record r;r.relative=safeName(readString(input));auto changed=read<uint8_t>(input),existed=read<uint8_t>(input);require(changed<=1&&existed<=1,"Invalid file flags.");r.changed=changed!=0;r.existed=existed!=0;r.size=read<uint64_t>(input);r.old=read<Digest>(input);r.next=read<Digest>(input);require(r.size<=24ULL*1024*1024*1024,"File too large.");total+=r.size;require(total<=24ULL*1024*1024*1024,"Update too large.");
        require(r.changed||(r.existed&&r.old==r.next),"Invalid retained file.");r.dest=inside(root,r.relative);r.stage=inside(session,fs::path(L"stage")/r.relative);r.backup=inside(session,fs::path(L"backup")/r.relative);names.push_back(lower(r.relative.wstring()));records.push_back(r);
    }
    require(input.peek()==std::char_traits<char>::eof(),"Trailing plan data.");std::sort(names.begin(),names.end());require(std::adjacent_find(names.begin(),names.end())==names.end(),"Duplicate game path.");
    for(auto name:{L"initialdunity.exe",L"unityplayer.dll",L"initialdunity_data\\globalgamemanagers",L"initialdunity_data\\managed\\assembly-csharp.dll"})require(std::binary_search(names.begin(),names.end(),name),"Incomplete game inventory.");
    return records;
}
static void verifyOld(const Record& r){noLinks(r.dest);require(fs::exists(r.dest)==r.existed,"Game files changed during update preparation.");if(r.existed)require(hash(r.dest)==r.old,"Game files changed during update preparation.");}
static uint64_t creation(HANDLE h){FILETIME a{},b{},c{},d{};require(GetProcessTimes(h,&a,&b,&c,&d)!=0,"Cannot identify the game process.");return (static_cast<uint64_t>(a.dwHighDateTime)<<32)|a.dwLowDateTime;}
static void noOtherGame(const fs::path& game,DWORD parent,bool strict=false){
    // Recovery must also work if the interrupted copy left no executable.
    auto expected=fs::exists(game)?canonicalExistingPath(game):canonicalExistingPath(game.parent_path())/game.filename();
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));require(snapshot.h!=INVALID_HANDLE_VALUE,"Cannot check running games.");PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
    if(Process32FirstW(snapshot.h,&entry))do{if(entry.th32ProcessID==parent||_wcsicmp(entry.szExeFile,L"InitialDUnity.exe"))continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID));if(!process.h){require(!strict||GetLastError()==ERROR_INVALID_PARAMETER,"Cannot check a running game. Close it before recovery.");continue;}
        wchar_t path[32768];DWORD length=32768;
        if(QueryFullProcessImageNameW(process.h,0,path,&length))require(lower(canonicalExistingPath(path).wstring())!=lower(expected.wstring()),"Another copy of this game is running.");
        else require(!strict,"Cannot identify a running game. Close it before recovery.");
    }while(Process32NextW(snapshot.h,&entry));
}
static void restart(const fs::path& root){
    auto exe=root/L"InitialDUnity.exe";std::wstring command=L"\""+exe.wstring()+L"\" -idas3-skip-update-once";
    STARTUPINFOW start{};start.cb=sizeof start;PROCESS_INFORMATION p{};
    require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&start,&p)!=0,"Update installed, but restarting the game failed. Launch it normally.");CloseHandle(p.hThread);CloseHandle(p.hProcess);
}
static void removeTree(const fs::path& session,const wchar_t* name){auto p=inside(session,name);noLinks(p);if(!fs::exists(p))return;for(const auto& entry:fs::recursive_directory_iterator(p))noLinks(entry.path());fs::remove_all(p);}
static void cleanupPayloads(const fs::path& session){
    // Each target is independent: a locked archive must not retain both copies
    // of the extracted game too. Never follow links out of this session.
    for(auto name:{L"stage",L"backup"})try{removeTree(session,name);}catch(...){}
    for(auto name:{L"game.zip",L"content.zip"})try{auto archive=inside(session,name);noLinks(archive);fs::remove(archive);}catch(...){}
}
static void result(const fs::path& session,bool committed,size_t changed,bool rollbackNeeded,bool recovered=false){
    // Publish a complete terminal classification for the managed scavenger.
    // A missing/partial result never authorizes deletion of recovery backups.
    auto temporary=inside(session,L"result.json.tmp"),destination=inside(session,L"result.json");
    write(temporary,"{\"schema\":1,\"passed\":"+std::string(committed?"true":"false")+",\"changedFiles\":"+std::to_string(changed)+",\"cleanupSafe\":"+(rollbackNeeded?"false":"true")+",\"rollbackNeeded\":"+(rollbackNeeded?"true":"false")+(recovered?",\"recovered\":true":"")+"}");
    noLinks(destination);require(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"Cannot publish update result.");
}
static std::string readStatus(const fs::path& path){
    noLinks(path);require(fs::is_regular_file(path)&&fs::file_size(path)<=32768,"Invalid update status file.");
    std::ifstream input(path,std::ios::binary);std::string value((std::istreambuf_iterator<char>(input)),{});
    require(!input.bad(),"Cannot read update status.");return value;
}
static int recover(const fs::path& requestedPlan,bool test){
    fs::path session;bool owned=false;
    Handle lease(INVALID_HANDLE_VALUE),installLock(INVALID_HANDLE_VALUE),gameLock(INVALID_HANDLE_VALUE);
    try {
        auto plan=fullPath(requestedPlan);require(plan.filename()==L"install.plan","Invalid install plan filename.");
        noLinks(plan);require(fs::is_regular_file(plan)&&fs::file_size(plan)<=64*1024*1024,"Invalid install plan.");
        plan=canonicalExistingPath(plan);session=plan.parent_path();
        auto leasePath=inside(session,L".cleanup-lock");noLinks(leasePath);
        lease.h=CreateFileW(leasePath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(lease.h!=INVALID_HANDLE_VALUE,"Update session cleanup is running.");
        auto installPath=inside(session,L".install-lock");noLinks(installPath);
        installLock.h=CreateFileW(installPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        require(installLock.h!=INVALID_HANDLE_VALUE,"This update session is already running.");
        std::ifstream input(plan,std::ios::binary);char magic[8]{};input.read(magic,8);
        require(bool(input)&&std::string(magic,8)=="IDUPD002","Invalid install plan.");
        auto root=canonicalExistingPath(fullPath(readString(input)));
        require(fs::is_directory(root),"Game directory missing.");
        require(lower(session.wstring()).rfind(lower(root.wstring()+L"\\"),0)!=0&&lower(root.wstring()).rfind(lower(session.wstring()+L"\\"),0)!=0&&lower(root.wstring())!=lower(session.wstring()),"Installer and game folders must not overlap.");
        auto pid=read<uint32_t>(input);auto stamp=read<uint64_t>(input);
        auto records=readRecords(input,root,session);
        if(test)require(fs::is_regular_file(root.parent_path()/L"ISOLATED_UPDATE_TEST.txt"),"Tests require an isolated fixture.");
        // A reused PID is harmless; the original process still being alive is not.
        if(pid){
            Handle parent(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
            if(parent.h)require(creation(parent.h)!=stamp||WaitForSingleObject(parent.h,0)==WAIT_OBJECT_0,"The original game is still running. Close it before recovery.");
            else require(GetLastError()==ERROR_INVALID_PARAMETER,"Cannot check the original game process.");
        }
        auto lockPath=root/L".update-lock";noLinks(lockPath);
        gameLock.h=CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        require(gameLock.h!=INVALID_HANDLE_VALUE,"Another update is running.");
        noOtherGame(root/L"InitialDUnity.exe",0,true);
        require(readStatus(session/L"ready")=="Prepared","The updater did not finish preparing its backups.");
        bool alreadyRecovered=false;
        auto terminal=inside(session,L"result.json");noLinks(terminal);
        if(fs::exists(terminal)){
            // Accept only the bounded schema emitted by the native installer.
            // Unknown, truncated or successful results never authorize rollback.
            auto value=readStatus(terminal);std::smatch fields;
            static const std::regex format(R"status(\s*\{\s*"schema"\s*:\s*1\s*,\s*"passed"\s*:\s*(true|false)\s*,\s*"changedFiles"\s*:\s*([0-9]{1,6})\s*,\s*"cleanupSafe"\s*:\s*(true|false)\s*,\s*"rollbackNeeded"\s*:\s*(true|false)\s*(,\s*"recovered"\s*:\s*true\s*)?\}\s*)status");
            require(std::regex_match(value,fields,format),"Unrecognized update result. Recovery backups were preserved.");
            require(std::stoul(fields[2].str())<=records.size(),"Invalid update result count.");
            alreadyRecovered=fields[5].matched;
            require(fields[1]=="false"&&((alreadyRecovered&&fields[3]=="true"&&fields[4]=="false")||(!alreadyRecovered&&fields[3]=="false"&&fields[4]=="true")),"This update has already completed; it cannot be recovered again.");
        }
        if(alreadyRecovered){for(const auto& record:records)verifyOld(record);return 0;}
        owned=true;
        // Verify the ENTIRE recovery set before changing any installed bytes.
        // Never depend on the damaged payload or on how far the old process got.
        for(const auto& record:records){
            noLinks(record.dest);noLinks(record.backup);
            require(!fs::exists(record.dest)||fs::is_regular_file(record.dest),"A recovery destination is not a file.");
            if(!record.changed)verifyOld(record);
            else if(record.existed)require(fs::is_regular_file(record.backup)&&hash(record.backup)==record.old,"A recovery backup is missing or damaged. No recovery files were replaced.");
        }
        noOtherGame(root/L"InitialDUnity.exe",0,true);
        size_t restored=0;
        for(const auto& record:records){
            if(!record.changed)continue;
            noLinks(record.dest);noLinks(record.backup);
            if(record.existed){
                require(hash(record.backup)==record.old,"Recovery backup changed. Remaining backups were preserved.");
                if(!fs::exists(record.dest)||hash(record.dest)!=record.old){
                    fs::create_directories(record.dest.parent_path());
                    require(CopyFileW(record.backup.c_str(),record.dest.c_str(),FALSE)!=0,"Cannot restore a game file. Close programs using the game and retry recovery.");
                    require(hash(record.dest)==record.old,"Restored file failed verification. Recovery can be retried.");
                }
            }else{fs::remove(record.dest);require(!fs::exists(record.dest),"Cannot remove a new update file.");}
            ++restored;
        }
        for(const auto& record:records)verifyOld(record);
        // Clearing stale errors before the atomic result is safe: an interrupted
        // recovery still has ready/backups and no cleanup-safe terminal marker.
        for(auto name:{L"error.txt",L"recovery-error.txt"}){auto path=inside(session,name);noLinks(path);fs::remove(path);}
        result(session,false,restored,false,true);
        // Keep backups until the game's normal cache cleanup sees this result.
        // Repeating recovery now verifies the old inventory without rewriting it.
        return 0;
    }catch(const std::exception& error){
        if(owned)try{write(session/L"recovery-error.txt",error.what());}catch(...){}
        auto message=std::string(error.what())+"\r\n";DWORD written=0;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE),message.data(),static_cast<DWORD>(message.size()),&written,nullptr);
        return 1;
    }
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);fs::path session,root;std::vector<Record> records;std::vector<size_t> applied;bool ready=false,committed=false,test=false,owned=false;
    if(argv&&((argc==3)||(argc==4&&std::wstring(argv[3])==L"--test"))&&std::wstring(argv[2])==L"--recover"){
        int code=recover(argv[1],argc==4);LocalFree(argv);return code;
    }
    // These handles must outlive the try block, including rollback and dialogs.
    Handle sessionLease(INVALID_HANDLE_VALUE),sessionLock(INVALID_HANDLE_VALUE),lockFile(INVALID_HANDLE_VALUE);
    try{
        require(argv&&(argc==2||argc==3),"Invalid installer arguments.");test=argc==3&&std::wstring(argv[2])==L"--test";require(argc!=3||test,"Invalid test option.");
        auto plan=fullPath(argv[1]);session=plan.parent_path();require(plan.filename()==L"install.plan","Invalid install plan filename.");noLinks(session);noLinks(plan);require(fs::file_size(plan)<=64*1024*1024,"Install plan too large.");
        plan=canonicalExistingPath(plan);session=plan.parent_path();
        auto lease=inside(session,L".cleanup-lock");noLinks(lease);
        sessionLease.h=CreateFileW(lease.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);require(sessionLease.h!=INVALID_HANDLE_VALUE,"Update session cleanup is running.");
        auto installerLock=inside(session,L".install-lock");noLinks(installerLock);
        sessionLock.h=CreateFileW(installerLock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE,nullptr);require(sessionLock.h!=INVALID_HANDLE_VALUE,"This update session is already running.");
        // Re-running a crashed or completed plan must not reclassify its old
        // rollback backup as an unused preparation that is safe to discard.
        for(auto name:{L"ready",L"result.json",L"result.json.tmp",L"error.txt"}){auto marker=inside(session,name);noLinks(marker);require(!fs::exists(marker),"This update session has already been used.");}
        std::ifstream input(plan,std::ios::binary);char magic[8];input.read(magic,8);require(std::string(magic,8)=="IDUPD002","Invalid install plan.");
        root=canonicalExistingPath(fullPath(readString(input)));auto game=root/L"InitialDUnity.exe";require(fs::is_regular_file(game),"Game executable missing.");
        require(lower(session.wstring()).rfind(lower(root.wstring()+L"\\"),0)!=0&&lower(root.wstring()).rfind(lower(session.wstring()+L"\\"),0)!=0&&lower(root.wstring())!=lower(session.wstring()),"Installer and game folders must not overlap.");
        // Only validated temporary locations may be marked safe or reclaimed.
        owned=true;
        auto pid=read<uint32_t>(input);auto stamp=read<uint64_t>(input);
        if(test)require(fs::is_regular_file(root.parent_path()/L"ISOLATED_UPDATE_TEST.txt"),"Tests require an isolated fixture.");else require(pid>0,"Game process missing.");
        Handle parent(pid?OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid):nullptr);
        if(pid){require(parent.h!=nullptr,"Game process unavailable.");require(creation(parent.h)==stamp,"Game process identity changed.");}
        auto lock=root/L".update-lock";noLinks(lock);lockFile.h=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE,nullptr);require(lockFile.h!=INVALID_HANDLE_VALUE,"Another update is running.");
        records=readRecords(input,root,session);
        if(!test)noOtherGame(game,pid);
        // Managed preparation already hashes retained files. Verify every file
        // again after the game exits, before any write; preflight only needs
        // changed files for the verified backups. Avoid a third full-game read.
        for(const auto& r:records){if(!r.changed)continue;verifyOld(r);noLinks(r.stage);noLinks(r.backup);require(fs::file_size(r.stage)==r.size&&hash(r.stage)==r.next,"Staged game file failed verification.");if(r.existed){fs::create_directories(r.backup.parent_path());require(CopyFileW(r.dest.c_str(),r.backup.c_str(),TRUE)!=0,"Cannot back up game file.");require(hash(r.backup)==r.old,"Backup verification failed.");}}
        write(session/L"ready","Prepared");ready=true;
        if(parent.h)require(WaitForSingleObject(parent.h,5*60*1000)==WAIT_OBJECT_0,"The game did not close; no files were replaced.");
        if(!test)noOtherGame(game,0);
        for(const auto& r:records)verifyOld(r);
        for(size_t i=0;i<records.size();++i){auto& r=records[i];if(!r.changed)continue;noLinks(r.dest);noLinks(r.stage);require(hash(r.stage)==r.next,"Staged file changed.");fs::create_directories(r.dest.parent_path());applied.push_back(i);require(CopyFileW(r.stage.c_str(),r.dest.c_str(),FALSE)!=0,"Cannot replace a game file.");require(hash(r.dest)==r.next,"Installed file failed verification.");}
        committed=true;result(session,true,applied.size(),false);
        // Restart may fail or immediately launch another cleanup pass. Reclaim
        // payloads first, with the lease held until this helper actually exits.
        cleanupPayloads(session);
        if(!test)restart(root);
        LocalFree(argv);return 0;
    }catch(const std::exception& e){
        bool rollbackNeeded=false;
        std::string error=e.what();if(!committed)for(auto it=applied.rbegin();it!=applied.rend();++it){auto& r=records[*it];try{noLinks(r.dest);if(r.existed){noLinks(r.backup);require(hash(r.backup)==r.old,"Invalid rollback backup.");if(!fs::exists(r.dest)||hash(r.dest)!=r.old)require(CopyFileW(r.backup.c_str(),r.dest.c_str(),FALSE)!=0,"Cannot restore game file.");require(hash(r.dest)==r.old,"Rollback hash mismatch.");}else{fs::remove(r.dest);require(!fs::exists(r.dest),"Cannot remove new game file during rollback.");}}catch(const std::exception& restore){rollbackNeeded=true;error+=" Rollback needs attention: ";error+=restore.what();}}
        if(owned){
            try{result(session,committed,applied.size(),rollbackNeeded);}catch(...){}
            try{write(session/L"error.txt",error);}catch(...){}
            if(!rollbackNeeded)cleanupPayloads(session);
        }
        if(ready&&!test){MessageBoxA(nullptr,error.c_str(),"Initial D update",MB_OK|MB_ICONERROR);if(!committed&&!rollbackNeeded)try{restart(root);}catch(...){}}
        if(argv)LocalFree(argv);return 1;
    }
}
