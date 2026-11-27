#pragma once
namespace idas3 {
// The Rev C v3sS00common bank omits the export frame at index 22.
constexpr unsigned originalCommonMenuChunk(unsigned index){
#ifdef IDAS3_JAPANESE_PLAYER
    return index-unsigned(index>22);
#else
    return index;
#endif
}
}
