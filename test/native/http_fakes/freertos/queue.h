#pragma once
#include "esp_stub.hpp"
#include "task.h"
#include <deque>
#include <vector>
struct FakeQueue {std::size_t capacity,size;std::deque<std::vector<unsigned char>> items;};
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t last_queue=nullptr;
inline std::vector<QueueHandle_t> fake_queues;
inline QueueHandle_t xQueueCreate(unsigned capacity,unsigned size){last_queue=new FakeQueue{capacity,size,{}};fake_queues.push_back(last_queue);return last_queue;}
inline int xQueueSend(QueueHandle_t q,const void* item,unsigned){if(q->items.size()>=q->capacity)return 0;const auto* p=static_cast<const unsigned char*>(item);q->items.emplace_back(p,p+q->size);return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void* item,unsigned){if(q->items.empty())return 0;std::memcpy(item,q->items.front().data(),q->size);q->items.pop_front();return pdTRUE;}
inline int xQueueReset(QueueHandle_t q){q->items.clear();return pdTRUE;}
