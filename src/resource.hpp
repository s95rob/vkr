#pragma once

#include <cstddef>
#include <unordered_map>

typedef size_t ResourceID;

template <typename T>
class ResourceRegistry {
public:
    ResourceRegistry() = default;

    template <typename ... TArgs>
    ResourceID Create(TArgs&& ... args) {
        ResourceID id = m_nextResourceID++;
        m_resources.emplace(id, T{std::forward<TArgs>(args) ...});
        return id;
    }

    void Destroy(ResourceID id) {
        m_resources.erase(id);
    }

    size_t GetSize() const { return m_resources.size(); }

    T& operator[](ResourceID id) { return m_resources[id]; }
    const T& operator[](ResourceID id) const { return m_resources[id]; }

private:
    ResourceID m_nextResourceID = 1;
    std::unordered_map<ResourceID, T> m_resources;
};