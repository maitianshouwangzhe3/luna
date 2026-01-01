/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux
#include <dirent.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <string>
#include <algorithm>
#include "luna.h"

struct luna_function_wapper final {
    luna_function_wapper(const lua_global_function& func) : m_func(func) {}
    lua_global_function m_func;
    DECLARE_LUA_CLASS(luna_function_wapper);
};

LUA_EXPORT_CLASS_BEGIN(luna_function_wapper)
LUA_EXPORT_CLASS_END()

static int lua_global_bridge(lua_State* L) {
    auto* wapper  = lua_to_object<luna_function_wapper*>(L, lua_upvalueindex(1));
    if (wapper != nullptr) {
        return wapper->m_func(L);
    }
    return 0;
}

void lua_push_function(lua_State* L, lua_global_function func) {
    lua_push_object(L, new luna_function_wapper(func));
    lua_pushcclosure(L, lua_global_bridge, 1);
}

int _lua_object_bridge(lua_State* L) {
    void* obj = lua_touserdata(L, lua_upvalueindex(1));
    lua_object_function* func = (lua_object_function*)lua_touserdata(L, lua_upvalueindex(2));
    if (obj != nullptr && func != nullptr) {
        return (*func)(obj, L);
    }
    return 0;
}

bool lua_get_table_function(lua_State* L, const char table[], const char function[]) {
    lua_getglobal(L, table);
    if (!lua_istable(L, -1))
        return false;
    lua_getfield(L, -1, function);
    lua_remove(L, -2);
    return lua_isfunction(L, -1);
}

bool lua_call_function(lua_State* L, std::string* err, int arg_count, int ret_count) {
    int func_idx = lua_gettop(L) - arg_count;
    if (func_idx <= 0 || !lua_isfunction(L, func_idx))
        return false;

    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_remove(L, -2); // remove 'debug'

    lua_insert(L, func_idx);
    if (lua_pcall(L, arg_count, ret_count, func_idx)) {
        if (err != nullptr) {
            *err = lua_tostring(L, -1);
        }
        lua_pop(L, 2);
        return false;
    }
    lua_remove(L, -ret_count - 1); // remove 'traceback'
    return true;
}

static const char* s_fence = "__fence__";

bool _lua_set_fence(lua_State* L, const char fence[]) {
    lua_getfield(L, LUA_REGISTRYINDEX, s_fence);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, s_fence);
    }

    if (lua_getfield(L, -1, fence) != LUA_TNIL) {
        lua_pop(L, 2);
        return false;
    }  
         
    lua_pushboolean(L, true);
    lua_setfield(L, -3, fence);
    lua_pop(L, 2);
    return true;     
}

void _lua_del_fence(lua_State* L, const char fence[]) {
    lua_getfield(L, LUA_REGISTRYINDEX, s_fence);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    
    lua_pushnil(L);
    lua_setfield(L, -2, fence);   
    lua_pop(L, 1);  
}

lua_table_object lua_table_to_object(lua_State* L, int idx) {
    std::unordered_map<std::string, boost::any> result;

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        // 只处理字符串键
        if (lua_type(L, -2) == LUA_TSTRING) {
            std::string key = lua_tostring(L, -2);

            boost::any value;
            switch (lua_type(L, -1)) {
                case LUA_TSTRING:
                    value = std::string(lua_tostring(L, -1));
                    break;
                case LUA_TNUMBER:
                    if (lua_isinteger(L, -1)) {
                        value = static_cast<long long>(lua_tointeger(L, -1));
                    } else {
                        value = static_cast<double>(lua_tonumber(L, -1));
                    }
                    break;
                case LUA_TBOOLEAN:
                    value = static_cast<bool>(lua_toboolean(L, -1));
                    break;
                case LUA_TNIL:
                    value = "nil";
                    break;
                case LUA_TTABLE:
                    // 递归处理嵌套 table
                    value = lua_table_to_object(L, -1);
                    break;
                default:
                    break;
            }

            if (!value.empty()) {
                result[key] = value;
            }
        }

        lua_pop(L, 1);  // remove value, keep key for next iteration
    }

    return result;
}