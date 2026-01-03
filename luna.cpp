/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#include <lua.h>
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

LUA_EXPORT_CLASS_BEGIN(lua_value)
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
    std::unordered_map<std::string, lua_value> result;

    luaL_checktype(L, idx, LUA_TTABLE);
    int absIndex = lua_absindex(L, idx);
    lua_pushnil(L);
    while (lua_next(L, absIndex) != 0) {
        // 只处理字符串键
        if (lua_type(L, -2) == LUA_TSTRING) {
            std::string key = lua_tostring(L, -2);
            switch (lua_type(L, -1)) {
                case LUA_TSTRING: {
                    std::string value = std::string(lua_tostring(L, -1));
                    lua_value val(value);
                    val.type = lua_value_type::LUA_OBJECT_TYPE_STRING;
                    result[key] = std::move(val);
                }
                    break;
                case LUA_TNUMBER: {
                    if (lua_isinteger(L, -1)) {
                        long long value = static_cast<long long>(lua_tointeger(L, -1));
                        lua_value val(value);
                        val.type = lua_value_type::LUA_OBJECT_TYPE_LONGLONG;
                        result[key] = std::move(val);
                    } else {
                        double value = static_cast<double>(lua_tonumber(L, -1));
                        lua_value val(value);
                        val.type = lua_value_type::LUA_OBJECT_TYPE_DOUBLE;
                        result[key] = std::move(val);
                    }
                }
                    break;
                case LUA_TBOOLEAN: {
                    bool value = static_cast<bool>(lua_toboolean(L, -1));
                    lua_value val(value);
                    val.type = lua_value_type::LUA_OBJECT_TYPE_BOOLEAN;
                    result[key] = std::move(val);
                }
                    break;
                case LUA_TNIL: {
                    lua_value val;
                    val.type = lua_value_type::LUA_OBJECT_TYPE_NONE;
                    result[key] = std::move(val);
                }
                    break;
                case LUA_TTABLE: {
                    // 递归处理嵌套 table
                    lua_table_object value = lua_table_to_object(L, -1);
                    lua_value val(value);
                    val.type = lua_value_type::LUA_OBJECT_TYPE_TABLE;
                    result[key] = std::move(val);
                }
                    break;
                default: {
                    lua_value val;
                    val.type = lua_value_type::LUA_OBJECT_TYPE_NONE;
                    result[key] = std::move(val);
                }
                    break;
            }
        }

        lua_pop(L, 1);
    }

    return result;
}

void cpp_object_to_table(lua_State* L, const lua_table_object& obj) {
    if (obj.empty() || obj.size() <= 0) {
        lua_pushnil(L);
        return;
    }

    lua_newtable(L);
    for (auto& item : obj) {
        lua_pushstring(L, item.first.data());
        switch (item.second.type) {
            case lua_value_type::LUA_OBJECT_TYPE_STRING: {
                const std::string& val = item.second.as<std::string>();
                lua_pushstring(L, val.data());
            }
            break;
            case lua_value_type::LUA_OBJECT_TYPE_DOUBLE: {
                lua_pushnumber(L, item.second.as<double>());
            }
            break;
            case lua_value_type::LUA_OBJECT_TYPE_LONGLONG: {
                lua_pushinteger(L, item.second.as<long long>());
            }
            break;
            break;
            case lua_value_type::LUA_OBJECT_TYPE_BOOLEAN: {
                lua_pushboolean(L, item.second.as<bool>());
            }
            break;
            case lua_value_type::LUA_OBJECT_TYPE_NONE: {
                lua_pushnil(L);
            }
            break;
            default:
                lua_pushnil(L);
            break;
        }
        lua_settable(L, -3);
    }
}