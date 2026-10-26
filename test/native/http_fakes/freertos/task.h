#pragma once
#include "esp_stub.hpp"
#include <functional>
constexpr int pdPASS=1,pdTRUE=1;
struct FakeStop {};
inline std::map<std::string,void(*)(void*)> fake_tasks;
inline std::function<void(unsigned)> fake_tick_hook;
inline unsigned fake_delay_count=0;
inline unsigned pdMS_TO_TICKS(unsigned x){return x;}
inline int xTaskCreate(void(*fn)(void*),const char* name,unsigned,void*,unsigned,void*){fake_tasks[name]=fn;return pdPASS;}
inline void vTaskDelay(unsigned ms){fake_time_us+=static_cast<std::int64_t>(ms)*1000;if(fake_tick_hook)fake_tick_hook(++fake_delay_count);}
inline void vTaskDelete(void*){throw FakeStop{};}
