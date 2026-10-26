#pragma once
#include <cstddef>
namespace openp1::http {
inline bool shallow_json(const char* data,std::size_t size) {
    unsigned depth=0;bool quoted=false,escaped=false;
    for(std::size_t i=0;i<size;++i){const char c=data[i];if(!c)return false;
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{'||c=='['){if(++depth>8)return false;}
        else if(c=='}'||c==']'){if(!depth)return false;--depth;}
    }
    return !quoted&&!depth;
}
}
