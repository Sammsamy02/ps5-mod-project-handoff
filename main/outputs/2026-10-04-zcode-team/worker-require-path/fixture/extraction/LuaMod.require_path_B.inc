    namespace
    {
        auto native_lua_push_text_call(lua_State* L) -> int
        {
            const auto* text = static_cast<const std::string_view*>(lua_touserdata(L, 1));
            lua_pushlstring(L, text->data(), text->size());
            return 1;
        }

        auto native_lua_push_text(lua_State* L, std::string_view text) -> int
        {
            // Zero-upvalue C functions and light userdata do not allocate.
            // Protect the string allocation so callers' C++ owners unwind
            // normally even when Lua reports an out-of-memory error.
            lua_pushcfunction(L, native_lua_push_text_call);
            lua_pushlightuserdata(L, &text);
            return lua_pcall(L, 1, 1, 0);
        }

        struct NativeLuaReader
        {
            std::string_view bytes;
            bool initial_newline;
        };

        auto native_lua_reader(lua_State*, void* context, size_t* size) -> const char*
        {
            auto& reader = *static_cast<NativeLuaReader*>(context);
            if (reader.initial_newline)
            {
                reader.initial_newline = false;
                *size = 1;
                return "\n";
            }
            *size = reader.bytes.size();
            const char* bytes = reader.bytes.data();
            reader.bytes = {};
            return *size ? bytes : nullptr;
        }

        auto native_lua_chunk(lua_State* L, std::string_view bytes, const char* chunk, const char* mode = nullptr) -> int
        {
            // Match luaL_loadfilex's BOM/shebang and source-line behavior. A
            // binary chunk must begin with its signature, without the newline.
            if (bytes.starts_with("\xef\xbb\xbf")) bytes.remove_prefix(3);
            bool newline = false;
            if (!bytes.empty() && bytes.front() == '#')
            {
                const auto end = bytes.find('\n');
                bytes = end == bytes.npos ? std::string_view{} : bytes.substr(end + 1);
                newline = bytes.empty() || bytes.front() != LUA_SIGNATURE[0];
            }
            NativeLuaReader reader{bytes, newline};
            // lua_load protects parser failures internally. The reader is POD;
            // the caller retains the NativeFileBuffer until this returns.
            return lua_load(L, native_lua_reader, &reader, chunk, mode);
        }

        auto native_lua_file(lua_State* L, const Platform::NativeModSource* source, std::string_view scripts,
                             const char* filename, size_t filename_size, const char* mode) -> int
        {
            // Errors are copied out before pushing Lua values: no C++ owner
            // survives a possible Lua allocation error or a later lua_error.
            char error[4608]{};
            {
                try
                {
                    if (!source) throw std::invalid_argument("native Lua source is unavailable");
                    if (!filename) throw std::invalid_argument("stdin is unavailable in native memory mode");
                    if (!filename_size || filename_size > 4096) throw std::invalid_argument("native Lua filename is empty or too long");
                    const std::string_view name(filename, filename_size);
                    // Relative names use this mod's Scripts/scripts directory.
                    // Canonical ue4ss/... keys are exact; source.read rejects
                    // absolute paths, parent components, NUL and Windows paths.
                    const std::string path = name.starts_with("ue4ss/") ? std::string(name) : std::string(scripts) + "/" + std::string(name);
                    const auto buffer = source->read(path);
                    if (buffer)
                    {
                        const std::string chunk = "@" + path;
                        return native_lua_chunk(L, buffer->bytes, chunk.c_str(), mode);
                    }
                    std::snprintf(error, sizeof(error), "cannot open native Lua file '%.*s': file is not retained",
                                  static_cast<int>(name.size()), name.data());
                }
                catch (const std::exception& exception)
                {
                    std::snprintf(error, sizeof(error), "cannot open native Lua file: %s", exception.what());
                }
                catch (...)
                {
                    std::snprintf(error, sizeof(error), "cannot open native Lua file: source lookup failed");
                }
            }
            lua_pushstring(L, error);
            return LUA_ERRFILE;
        }

        auto native_lua_loadfile(lua_State* L) -> int
        {
            size_t filename_size = 0, scripts_size = 0;
            const char* filename = luaL_optlstring(L, 1, nullptr, &filename_size);
            const char* mode = luaL_optstring(L, 2, nullptr);
            const int environment = lua_isnone(L, 3) ? 0 : 3;
            const auto* source = static_cast<const Platform::NativeModSource*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* scripts = lua_tolstring(L, lua_upvalueindex(2), &scripts_size);
            const int status = native_lua_file(L, source, std::string_view(scripts, scripts_size), filename, filename_size, mode);
            if (status == LUA_OK)
            {
                if (environment)
                {
                    lua_pushvalue(L, environment);
                    if (!lua_setupvalue(L, -2, 1)) lua_pop(L, 1);
                }
                return 1;
            }
            luaL_pushfail(L);
            lua_insert(L, -2);
            return 2;
        }

        auto native_lua_dofile_continue(lua_State* L, int, lua_KContext) -> int
        {
            return lua_gettop(L) - 1;
        }

        auto native_lua_dofile(lua_State* L) -> int
        {
            size_t filename_size = 0, scripts_size = 0;
            const char* filename = luaL_optlstring(L, 1, nullptr, &filename_size);
            lua_settop(L, 1);
            const auto* source = static_cast<const Platform::NativeModSource*>(lua_touserdata(L, lua_upvalueindex(1)));
            const char* scripts = lua_tolstring(L, lua_upvalueindex(2), &scripts_size);
            if (native_lua_file(L, source, std::string_view(scripts, scripts_size), filename, filename_size, nullptr) != LUA_OK)
                return lua_error(L);
            // Only borrowed pointers/POD remain. Yield and runtime errors can
            // cross this frame without bypassing C++ owned-buffer destructors.
            lua_callk(L, 0, LUA_MULTRET, 0, native_lua_dofile_continue);
            return native_lua_dofile_continue(L, 0, 0);
        }

        // Return -1 with a Lua error on the stack for a malformed existing
        // module. The caller raises it after these C++ owners have unwound.
        auto native_lua_module(lua_State* L, const Platform::NativeModSource& source, const std::string& scripts_path) -> int
        {
            if (lua_type(L, 1) != LUA_TSTRING)
            {
                return native_lua_push_text(L, "module name must be a string") == LUA_OK ? 1 : -1;
            }
            size_t name_size = 0;
            const char* name = lua_tolstring(L, 1, &name_size);
            std::string relative(name, name_size);
            bool valid = !relative.empty() && relative.size() <= 4096;
            for (char& c : relative)
            {
                const auto byte = static_cast<unsigned char>(c);
                if (byte < 0x20 || byte == 0x7f || c == '\\' || c == ':') valid = false;
                if (c == '.') c = '/';
            }
            if (!valid || relative.front() == '/' || relative.back() == '/' || relative.find("//") != relative.npos)
            {
                return native_lua_push_text(L, "invalid native Lua module name") == LUA_OK ? 1 : -1;
            }
            const std::string shared = "ue4ss/Mods/shared/";
            const std::vector<std::string> paths{
                scripts_path + "/" + relative + ".lua",
                scripts_path + "/" + relative + "/init.lua",
                shared + relative + ".lua",
                shared + relative + "/init.lua",
                shared + relative + "/" + relative + ".lua",
            };
            std::string missing;
            for (const auto& path : paths)
            {
                const auto buffer = source.read(path);
                if (!buffer)
                {
                    missing += "\n\tno native Lua file '" + path + "'";
                    continue;
                }
                const std::string chunk = "@" + path;
                // NativeFileBuffer holds the immutable owner through parsing.
                // Return the chunk directly: Lua require supplies module name
                // and loader data, and owns standard package.loaded caching.
                if (native_lua_chunk(L, buffer->bytes, chunk.c_str()) != LUA_OK) return -1;
                return native_lua_push_text(L, path) == LUA_OK ? 2 : -1;
            }
            return native_lua_push_text(L, missing) == LUA_OK ? 1 : -1;
        }
    }
    // END native Lua buffer helpers
