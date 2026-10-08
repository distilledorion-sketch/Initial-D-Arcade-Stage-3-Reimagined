#include "native_asset_packing.h"
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace fs=std::filesystem;
using namespace idas3;
namespace {
void plainPath(const fs::path& path) {
    for (auto p=path;!p.empty();) {
        if (fs::is_symlink(p)) throw std::runtime_error("Packing through a link is forbidden");
#ifdef _WIN32
        auto attributes=GetFileAttributesW(p.c_str());
        if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Packing through a reparse path is forbidden");
#endif
        auto parent=p.parent_path();if (parent==p) break;p=parent;
    }
}
bool nested(const fs::path& a,const fs::path& b) {
    for (auto p=a;!p.empty();) {
        if (fs::equivalent(p,b)) return true;
        auto parent=p.parent_path();if(parent==p)break;p=parent;
    }
    return false;
}
std::vector<char> read(const fs::path& path) {
    auto length=fs::file_size(path);
    if (!length || length>nativeAssetMaxSize) throw std::runtime_error("Invalid native source size");
    std::vector<char> out(std::size_t(length),0);
    std::ifstream input(path,std::ios::binary);
    if (!input.read(out.data(),std::streamsize(out.size()))) throw std::runtime_error("Cannot read native source");
    return out;
}
void verify(const fs::path& path,std::span<const char> expected) {
    NativeAssetReader reader(path);std::vector<char> block(nativeAssetBlockSize);
    for (std::size_t offset=0;offset<expected.size();offset+=block.size()) {
        auto count=std::min(block.size(),expected.size()-offset);reader.bytes(block.data(),count);
        if (!std::equal(block.begin(),block.begin()+count,expected.begin()+offset))
            throw std::runtime_error("Packed native asset changed decoded bytes");
    }
    if (!reader.end()) throw std::runtime_error("Packed native asset has trailing bytes");
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {try {
#else
int main(int argc,char** argv) {try {
#endif
    if (argc!=3) throw std::runtime_error("Usage: pack_native_assets SOURCE_DATA STAGED_DATA");
    auto source=fs::absolute(argv[1]).lexically_normal(),destination=fs::absolute(argv[2]).lexically_normal();
    plainPath(source);plainPath(destination);
    if (!fs::is_directory(source)||!fs::is_directory(destination)||nested(source,destination)||nested(destination,source))
        throw std::runtime_error("Source and staged data must be separate existing trees");
    // Validate the entire traversal before changing any staged file.
    std::vector<fs::path> assets;
    for (const auto& entry:fs::recursive_directory_iterator(source)) {
        plainPath(entry.path());
        if (entry.is_regular_file() && (entry.path().extension()==".idastex"||entry.path().extension()==".idasmesh")) {
            auto relative=entry.path().lexically_relative(source);auto target=destination/relative;
            plainPath(target);
            if (!fs::is_regular_file(target)) throw std::runtime_error("Native assets must be staged before packing");
            assets.push_back(relative);
        }
    }
    std::sort(assets.begin(),assets.end());
    std::uint64_t before=0,after=0,changed=0;
    for (const auto& relative:assets) {
        auto raw=read(source/relative);
        if (raw.size()>=8 && std::equal(nativePackedMagic.begin(),nativePackedMagic.end(),raw.begin()))
            throw std::runtime_error("Packing requires the original uncompressed source snapshot");
        auto target=destination/relative;verify(target,raw);
        auto packed=packNativeAsset(raw);before+=raw.size();after+=packed.size();
        if (packed.size()<raw.size()) {
            auto temporary=target;temporary+=".packing";
            if (fs::exists(temporary)) throw std::runtime_error("Packing temporary already exists");
            {
                std::ofstream output(temporary,std::ios::binary);
                output.write(packed.data(),std::streamsize(packed.size()));output.close();
                if (!output) throw std::runtime_error("Writing packed native asset failed");
            }
            verify(temporary,raw);
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Replacing staged native asset failed");
#else
            fs::rename(temporary,target);
#endif
            ++changed;
        }
    }
    std::cout<<"{\"filesVerified\":"<<assets.size()<<",\"packedFiles\":"<<changed
        <<",\"sourceBytes\":"<<before<<",\"storedBytes\":"<<after<<",\"savedBytes\":"<<(before-after)<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
