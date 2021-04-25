#pragma once

#include <Lua/LuaHelpers.h>
#include <Lua/Shared/LuaLifetime.h>

namespace bg3se::lua
{
	LifetimeHolder GetCurrentLifetime();

	template <class T>
	class LuaPropertyMap
	{
	public:
		struct PropertyAccessors
		{
			using Getter = bool (lua_State* L, LifetimeHolder const& lifetime, T* object);
			using Setter = bool (lua_State* L, LifetimeHolder const& lifetime, T* object, int index);

			Getter* Get;
			Setter* Set;
		};

		bool GetProperty(lua_State* L, LifetimeHolder const& lifetime, T* object, STDString const& prop) const
		{
			auto it = Properties.find(prop);
			if (it == Properties.end()) {
				return 0;
			}

			return it->second.Get(L, lifetime, object);
		}

		bool SetProperty(lua_State* L, LifetimeHolder const& lifetime, T* object, STDString const& prop, int index) const
		{
			auto it = Properties.find(prop);
			if (it == Properties.end()) {
				return 0;
			}

			return it->second.Set(L, lifetime, object, index);
		}

		void AddProperty(STDString const& prop, typename PropertyAccessors::Getter* getter, typename PropertyAccessors::Setter* setter)
		{
			Properties.insert(std::make_pair(prop, PropertyAccessors{ getter, setter }));
		}

		std::unordered_map<STDString, PropertyAccessors> Properties;
		std::vector<STDString> Parents;
	};

	template <class T>
	struct StaticLuaPropertyMap
	{
		using ObjectType = T;

		static LuaPropertyMap<T> PropertyMap;
	};

	class ObjectProxyImplBase
	{
	public:
		inline virtual ~ObjectProxyImplBase() {};
		virtual char const* GetTypeName() const = 0;
		virtual void* GetRaw() = 0;
		virtual bool GetProperty(lua_State* L, char const* prop) = 0;
		virtual bool SetProperty(lua_State* L, char const* prop, int index) = 0;
		virtual int Next(lua_State* L, char const* key) = 0;
		virtual bool IsA(char const* typeName) = 0;
	};

	template <class T>
	struct ObjectProxyHelpers
	{
		static char const* const TypeName;

		static bool GetProperty(lua_State* L, T* object, LifetimeHolder const& lifetime, char const* prop)
		{
			auto const& map = StaticLuaPropertyMap<T>::PropertyMap;
			auto fetched = map.GetProperty(L, lifetime, object, prop);
			if (!fetched) {
				luaL_error(L, "Object of type '%s' has no property named '%s'", TypeName, prop);
				return false;
			}

			return true;
		}

		static bool SetProperty(lua_State* L, T* object, LifetimeHolder const& lifetime, char const* prop, int index)
		{
			auto const& map = StaticLuaPropertyMap<T>::PropertyMap;
			auto ok = map.SetProperty(L, lifetime, object, prop, index);
			if (!ok) {
				luaL_error(L, "Object of type '%s' has no property named '%s'", TypeName, prop);
				return false;
			}

			return true;
		}

		static int Next(lua_State* L, T* object, LifetimeHolder const& lifetime, char const* key)
		{
			auto const& map = StaticLuaPropertyMap<T>::PropertyMap;
			if (key == nullptr) {
				if (!map.Properties.empty()) {
					StackCheck _(L, 2);
					auto it = map.Properties.begin();
					push(L, it->first);
					if (!it->second.Get(L, lifetime, object)) {
						push(L, nullptr);
					}

					return 2;
				}
			}
			else {
				auto it = map.Properties.find(key);
				if (it != map.Properties.end()) {
					++it;
					if (it != map.Properties.end()) {
						StackCheck _(L, 2);
						push(L, it->first);
						if (!it->second.Get(L, lifetime, object)) {
							push(L, nullptr);
						}

						return 2;
					}
				}
			}

			return 0;
		}

		static bool IsA(char const* typeName)
		{
			if (strcmp(typeName, TypeName) == 0) {
				return true;
			}

			auto const& map = StaticLuaPropertyMap<T>::PropertyMap;
			for (auto const& parent : map.Parents) {
				if (strcmp(parent.c_str(), typeName) == 0) {
					return true;
				}
			}

			return false;
		}
	};

	template <class T>
	class ObjectProxyRefImpl : public ObjectProxyImplBase
	{
	public:
		static_assert(!std::is_pointer_v<T>, "ObjectProxyImpl template parameter should not be a pointer type!");

		ObjectProxyRefImpl(LifetimeHolder const& lifetime, T * obj)
			: object_(obj), lifetime_(lifetime)
		{}
		
		~ObjectProxyRefImpl() override
		{}

		T* Get() const
		{
			return object_;
		}

		void* GetRaw() override
		{
			return object_;
		}

		char const* GetTypeName() const override
		{
			return ObjectProxyHelpers<T>::TypeName;
		}

		bool GetProperty(lua_State* L, char const* prop) override
		{
			return ObjectProxyHelpers<T>::GetProperty(L, object_, lifetime_, prop);
		}

		bool SetProperty(lua_State* L, char const* prop, int index) override
		{
			return ObjectProxyHelpers<T>::SetProperty(L, object_, lifetime_, prop, index);
		}

		int Next(lua_State* L, char const* key) override
		{
			return ObjectProxyHelpers<T>::Next(L, object_, lifetime_, key);
		}

		bool IsA(char const* typeName) override
		{
			return ObjectProxyHelpers<T>::IsA(typeName);
		}

	private:
		T* object_;
		LifetimeHolder lifetime_;
	};


	// Object proxy that owns the contained object and deletes the object on GC
	template <class T>
	class ObjectProxyOwnerImpl : public ObjectProxyImplBase
	{
	public:
		static_assert(!std::is_pointer_v<T>, "ObjectProxyImpl template parameter should not be a pointer type!");

		ObjectProxyOwnerImpl(LifetimePool& pool, Lifetime* lifetime, T* obj)
			: lifetime_(pool, lifetime), 
			object_(obj, &GameDelete<T>)
		{}

		~ObjectProxyOwnerImpl() override
		{
			lifetime_.GetLifetime()->Kill();
		}

		T* Get() const
		{
			return object_.get();
		}

		void* GetRaw() override
		{
			return object_.get();
		}

		char const* GetTypeName() const override
		{
			return ObjectProxyHelpers<T>::TypeName;
		}

		bool GetProperty(lua_State* L, char const* prop) override
		{
			return ObjectProxyHelpers<T>::GetProperty(L, object_.get(), lifetime_.Get(), prop);
		}

		bool SetProperty(lua_State* L, char const* prop, int index) override
		{
			return ObjectProxyHelpers<T>::SetProperty(L, object_.get(), lifetime_.Get(), prop, index);
		}

		int Next(lua_State* L, char const* key) override
		{
			return ObjectProxyHelpers<T>::Next(L, object_.get(), lifetime_.Get(), key);
		}

		bool IsA(char const* typeName) override
		{
			return ObjectProxyHelpers<T>::IsA(typeName);
		}

	private:
		GameUniquePtr<T> object_;
		LifetimeReference lifetime_;
	};


	class ObjectProxy : private Userdata<ObjectProxy>, public Indexable, public NewIndexable, 
		public Iterable, public Pushable, public GarbageCollected
	{
	public:
		static char const * const MetatableName;

		template <class T>
		inline static ObjectProxyRefImpl<T>* MakeRef(lua_State* L, T* object, LifetimeHolder const& lifetime)
		{
			static_assert(sizeof(ObjectProxyRefImpl<T>) <= sizeof(impl_), "ObjectProxy implementation object too large!");
			auto self = New(L, lifetime);
			return new (self->impl_) ObjectProxyRefImpl<T>(lifetime, object);
		}

		template <class T>
		inline static ObjectProxyOwnerImpl<T>* MakeOwner(lua_State* L, LifetimePool& pool, T* obj)
		{
			static_assert(sizeof(ObjectProxyOwnerImpl<T>) <= sizeof(impl_), "ObjectProxy implementation object too large!");
			auto lifetime = pool.Allocate();
			auto self = New(L, LifetimeHolder(pool, lifetime));
			return new (self->impl_) ObjectProxyOwnerImpl<T>(pool, lifetime, obj);
		}

		template <class T, class... Args>
		inline static ObjectProxyOwnerImpl<T>* MakeOwner(lua_State* L, LifetimePool& pool, Args... args)
		{
			static_assert(sizeof(ObjectProxyOwnerImpl<T>) <= sizeof(impl_), "ObjectProxy implementation object too large!");
			auto lifetime = pool.Allocate();
			auto self = New(L, LifetimeHolder(pool, lifetime));
			auto obj = GameAlloc<T, Args...>(std::forward(args)...);
			return new (self->impl_) ObjectProxyOwnerImpl<T>(pool, lifetime, obj);
		}

		inline ObjectProxyImplBase* GetImpl()
		{
			return reinterpret_cast<ObjectProxyImplBase*>(impl_);
		}

		inline bool IsAlive() const
		{
			return lifetime_.IsAlive();
		}

		template <class T>
		T* Get()
		{
			if (!lifetime_.IsAlive()) {
				return nullptr;
			}

			if (GetImpl()->IsA(ObjectProxyHelpers<T>::TypeName)) {
				return reinterpret_cast<T*>(GetImpl()->GetRaw());
			} else {
				return nullptr;
			}
		}

	private:
		LifetimeReference lifetime_;
		uint8_t impl_[40];

		ObjectProxy(LifetimeHolder const& lifetime)
			: lifetime_(lifetime)
		{}

		~ObjectProxy()
		{
			GetImpl()->~ObjectProxyImplBase();
		}

	protected:
		friend Userdata<ObjectProxy>;

		int Index(lua_State* L)
		{
			StackCheck _(L, 1);
			auto impl = GetImpl();
			if (!lifetime_.IsAlive()) {
				luaL_error(L, "Attempted to read dead object of type '%s'", impl->GetTypeName());
				push(L, nullptr);
				return 1;
			}

			auto prop = luaL_checkstring(L, 2);
			if (!impl->GetProperty(L, prop)) {
				push(L, nullptr);
			}

			return 1;
		}

		int NewIndex(lua_State* L)
		{
			StackCheck _(L, 0);
			auto impl = GetImpl();
			if (!lifetime_.IsAlive()) {
				luaL_error(L, "Attempted to write dead object of type '%s'", impl->GetTypeName());
				return 0;
			}

			auto prop = luaL_checkstring(L, 2);
			impl->SetProperty(L, prop, 3);
			return 0;
		}

		int Next(lua_State* L)
		{
			auto impl = GetImpl();
			if (!lifetime_.IsAlive()) {
				luaL_error(L, "Attempted to iterate dead object of type '%s'", impl->GetTypeName());
				return 0;
			}

			if (lua_type(L, 2) == LUA_TNIL) {
				return impl->Next(L, nullptr);
			} else {
				auto key = checked_get<char const*>(L, 2);
				return impl->Next(L, key);
			}
		}

		int GC(lua_State* L)
		{
			this->~ObjectProxy();
			return 0;
		}
	};

	template <class T>
	inline void push_proxy(lua_State* L, LifetimeHolder const& lifetime, T const& v)
	{
		if constexpr (std::is_pointer_v<T> 
			&& (std::is_base_of_v<HasObjectProxy, std::remove_pointer_t<T>>
				|| HasObjectProxyTag<std::remove_pointer_t<T>>::HasProxy)) {
			if (v) {
				ObjectProxy::MakeRef<std::remove_pointer_t<T>>(L, v, lifetime);
			} else {
				lua_pushnil(L);
			}
		} else {
			push(L, v);
		}
	}

	template <class T>
	inline T* checked_get_proxy(lua_State* L, int index)
	{
		auto proxy = Userdata<ObjectProxy>::CheckUserData(L, index);
		auto const& typeName = ObjectProxyHelpers<T>::TypeName;
		if (proxy->GetImpl()->IsA(typeName)) {
			auto obj = proxy->Get<T>();
			if (obj == nullptr) {
				luaL_error(L, "Argument %d: got object of type '%s' whose lifetime has expired", index, typeName);
				return nullptr;
			} else {
				return obj;
			}
		} else {
			luaL_error(L, "Argument %d: expected an object of type '%s', got '%s'", index, typeName, proxy->GetImpl()->GetTypeName());
			return nullptr;
		}
	}
}