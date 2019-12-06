#include "router.h"
#include <stdint.h>
#include <stdlib.h>



/*
    RoutingTable Entry 的定义如下：
    typedef struct {
        uint32_t addr; // 大端序，IPv4 地址
        uint32_t len; // 小端序，前缀长度
        uint32_t if_index; // 小端序，出端口编号
        uint32_t nexthop; // 大端序，下一跳的 IPv4 地址
    } RoutingTableEntry;

    约定 addr 和 nexthop 以 **大端序** 存储。
    这意味着 1.2.3.4 对应 0x04030201 而不是 0x01020304。
    保证 addr 仅最低 len 位可能出现非零。
    当 nexthop 为零时这是一条直连路由。
    你可以在全局变量中把路由表以一定的数据结构格式保存下来。
*/


//#define NAIVE

#ifndef NAIVE
#include <assert.h>
#include <boost/unordered_map.hpp>
typedef boost::unordered_map<uint32_t, RoutingTableEntry> table_t;
table_t table[33]; 
//#define MASK 0xffffffff
const uint32_t MASK = 0xffffffff;

void update(bool insert, RoutingTableEntry entry) {
    auto itr = table[entry.len].find(entry.addr & (MASK >> 32 - entry.len));
    if (insert) {
        if (itr == table[entry.len].end()) { table[entry.len].insert(std::make_pair(entry.addr & (MASK >> 32 - entry.len), entry)); }
        else {
            itr->second.if_index = entry.if_index;
            itr->second.nexthop = entry.nexthop;
        }
    } else {
        if (itr != table[entry.len].end()) {
            table[entry.len].erase(itr);
        }
    }
}

bool query(uint32_t addr, uint32_t *nexthop, uint32_t *if_index) {
    for (uint32_t i = 0; i < 32; ++i) {
        auto itr = table[32 - i].find(addr & (MASK >> i));
        if (itr == table[32 - i].end()) continue;
        else {
            *nexthop = itr->second.nexthop;
            *if_index = itr->second.if_index;
            return true;
        }
    }
    return false;
}

#endif

#ifdef NAIVE 
#include <vector>
std::vector<RoutingTableEntry> routing_table;

inline uint32_t right_zero(uint32_t x) {
    int n = 0;
    uint32_t y = 0xffffffff;
    y >>= 16; if (!(x & y)) { n += 16; x >>= 16;}
    y >>= 8; if (!(x & y)) { n += 8; x >>= 8;}
    y >>= 4; if (!(x & y)) { n += 4; x >>= 4;}
    y >>= 2; if (!(x & y)) { n += 2; x >>= 2;}
    y >>= 1; if (!(x & y)) { n += 1; x >>= 1;}
    if (x == 0) {n += 1;}
    return n;
}

/**
 * @brief 插入/删除一条路由表表项
 * @param insert 如果要插入则为 true ，要删除则为 false
 * @param entry 要插入/删除的表项
 * 
 * 插入时如果已经存在一条 addr 和 len 都相同的表项，则替换掉原有的。
 * 删除时按照 addr 和 len 匹配。
 */
void update(bool insert, RoutingTableEntry entry) {
    for (int i = 0; i < routing_table.size(); ++i) {
        if (routing_table[i].addr == entry.addr && routing_table[i].len == entry.len) {
            //update
            if (insert) routing_table[i] = entry;
            else routing_table.erase(routing_table.begin() + i);
            return;
        }   
    }
    if (insert) routing_table.push_back(entry);
}

/**
 * @brief 进行一次路由表的查询，按照最长前缀匹配原则
 * @param addr 需要查询的目标地址，大端序
 * @param nexthop 如果查询到目标，把表项的 nexthop 写入
 * @param if_index 如果查询到目标，把表项的 if_index 写入
 * @return 查到则返回 true ，没查到则返回 false
 */
bool query(uint32_t addr, uint32_t *nexthop, uint32_t *if_index) {
    int max_idx = -1, max_len = 0;
    for (int i = 0; i< routing_table.size() ; ++i) {
        int zero_num = right_zero(addr ^ routing_table[i].addr);
        if (zero_num > max_len) {
            max_idx = i;
            max_len = zero_num;
        }
    }
    if (max_idx >= 0) {
        *nexthop = routing_table[max_idx].nexthop;
        *if_index = routing_table[max_idx].if_index;
        return true;
    } else return false;
}
#endif
