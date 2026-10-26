#pragma once
#include "esp_stub.hpp"
#include <vector>
using nvs_handle_t=int;
constexpr int NVS_READONLY=0,NVS_READWRITE=1,ESP_ERR_NVS_NOT_FOUND=2;
inline bool fake_commit_ok=true;
inline std::map<std::string,std::vector<unsigned char>> fake_nvs,fake_pending_nvs;
inline int nvs_open(const char*,int,nvs_handle_t* n){*n=1;return ESP_OK;}
inline int nvs_get_blob(nvs_handle_t,const char* key,void* data,std::size_t* size){auto i=fake_nvs.find(key);if(i==fake_nvs.end())return ESP_ERR_NVS_NOT_FOUND;if(*size<i->second.size())return ESP_FAIL;*size=i->second.size();std::memcpy(data,i->second.data(),*size);return ESP_OK;}
inline int nvs_set_blob(nvs_handle_t,const char* key,const void* data,std::size_t size){const auto* p=static_cast<const unsigned char*>(data);fake_pending_nvs[key]={p,p+size};return ESP_OK;}
inline int nvs_commit(nvs_handle_t){if(!fake_commit_ok){fake_pending_nvs.clear();return ESP_FAIL;}for(const auto& p:fake_pending_nvs)fake_nvs[p.first]=p.second;fake_pending_nvs.clear();return ESP_OK;}
inline void nvs_close(nvs_handle_t){}

inline int nvs_set_u8(nvs_handle_t n,const char* key,unsigned char value){return nvs_set_blob(n,key,&value,1);}
