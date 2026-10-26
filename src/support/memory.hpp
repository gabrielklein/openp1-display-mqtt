#pragma once
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
namespace openp1::memory {
// ESP-IDF builds without C++ exceptions may abort even for new(std::nothrow).
// Check malloc directly, then use non-allocating placement construction.
using Allocator=void*(*)(std::size_t);
template<class T> struct Destroy {
    void operator()(T* p) const { if(p){p->~T();std::free(p);} }
};
template<class T> using Pointer=std::unique_ptr<T,Destroy<T>>;
template<class T> Pointer<T> make(Allocator allocate=std::malloc){
    static_assert(std::is_nothrow_default_constructible<T>::value,"Construction must not throw");
    void* storage=allocate(sizeof(T));
    return Pointer<T>(storage?::new(storage) T{}:nullptr);
}
struct Free {void operator()(char* p) const {std::free(p);}};
inline std::unique_ptr<char,Free> buffer(std::size_t size,Allocator allocate=std::malloc){
    return std::unique_ptr<char,Free>(static_cast<char*>(allocate(size)));
}
}
