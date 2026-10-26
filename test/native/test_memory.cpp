#include "support/memory.hpp"
#include <cassert>
namespace {
unsigned destroyed=0;
struct Object {int value=42;~Object(){++destroyed;}};
void* fail(std::size_t){return nullptr;}
}
int main(){
    assert(!openp1::memory::make<Object>(fail));
    assert(!openp1::memory::buffer(8193,fail));
    assert(destroyed==0);
    {auto object=openp1::memory::make<Object>();assert(object&&object->value==42);}
    assert(destroyed==1);
    auto bytes=openp1::memory::buffer(8193);assert(bytes);bytes.get()[8192]=0;
}
