#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

template <class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
 public:
    typedef std::pair<const KeyType, ValueType> value_type;
    typedef Hash hasher;
    size_t capacity() const { return _capacity; }

    HashMap& operator=(const HashMap& other) {
        HashMap temp(other);
        if (this != &other) {
            temp.swap(*this);
        }
        return *this;
    }

    void swap(HashMap& other) {
        std::swap(_hash, other._hash);
        std::swap(_size, other._size);
        std::swap(_capacity, other._capacity);
        items.swap(other.items);
    }

    HashMap(const HashMap& other) {
        clear();
        _hash = other._hash;
        _capacity = kDefaultCapacity;
        items.assign(kDefaultCapacity, nullptr);
        for (int i = 0; i < other._capacity; ++i) {
            if (other.items[i] && other.items[i]->state == item_state::USED) {
                add(other.items[i]->el, items, _capacity);
                check_rehash();
            }
        }
    }

    HashMap(size_t capacity, Hash new_hash = Hash())
        : _size(0),
          _capacity(capacity),
          _hash(new_hash),
          items(std::vector<std::shared_ptr<Item>>(capacity, nullptr)) {}

    HashMap(Hash new_hash = Hash()) : HashMap(kDefaultCapacity, new_hash) {}

    template <class _Iter>
    HashMap(_Iter First, _Iter Last, Hash new_hash = Hash()) : _hash(new_hash) {
        _size = 0;
        _capacity = kDefaultCapacity;
        items.assign(_capacity, nullptr);
        while (First != Last) {
            add(*First, items, _capacity);
            check_rehash();
            ++First;
        }
    }

    HashMap(std::initializer_list<std::pair<const KeyType, ValueType>> list,
            Hash new_hash = Hash())
        : _hash(new_hash) {
        _size = 0;
        _capacity = kDefaultCapacity;
        items.assign(_capacity, nullptr);
        for (auto& x : list) {
            add(x, items, _capacity);
            check_rehash();
        }
    }

    size_t size() const { return _size; }

    bool empty() const { return size() == 0; }

    Hash hash_function() const { return _hash; }

    void insert(const std::pair<KeyType, ValueType>& item) {
        add(item, items, _capacity);
        check_rehash();
    }
    void erase(const KeyType& key) {
        size_t index = get_index(key, _capacity);
        for (int i = 0; i < _capacity; ++i) {
            if (items[index] == 0 ||
                (items[index]->state == item_state::ERASED &&
                 items[index]->el.first == key)) {
                return;
            }
            if (items[index]->state == item_state::USED &&
                items[index]->el.first == key) {
                items[index]->state = item_state::ERASED;
                --_size;
                check_rehash_erase();
                return;
            }
            ++index;
            if (index == _capacity) {
                index = 0;
            }
        }
    }

    class iterator : public std::iterator<std::forward_iterator_tag,
                                          std::pair<const KeyType, ValueType>> {
     public:
        iterator(HashMap<KeyType, ValueType, Hash>* _owner = nullptr,
                 size_t _index = 0)
            : index(_index), owner(_owner) {}
        std::pair<const KeyType, ValueType>& operator*() {
            return owner->items[index]->el;
        }
        std::pair<const KeyType, ValueType>* operator->() { return &**this; }

        iterator& operator++() {
            ++index;
            for (int i = index; i < owner->_capacity; ++i) {
                if (owner->items[i] &&
                    owner->items[i]->state == item_state::USED) {
                    break;
                }
                ++index;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++index;
            for (int i = index; i < owner->_capacity; ++i) {
                if (owner->items[i] &&
                    owner->items[i]->state == item_state::USED) {
                    break;
                }
                ++index;
            }
            return temp;
        }

        bool operator==(const iterator& other) const {
            return owner == other.owner && index == other.index;
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

     private:
        size_t index;
        HashMap<KeyType, ValueType, Hash>* owner;
    };

    iterator begin() {
        int index = 0;
        for (int i = 0; i < _capacity; ++i) {
            if (items[i] && items[i]->state == item_state::USED) {
                break;
            }
            ++index;
        }
        return iterator(this, index);
    }
    iterator end() { return iterator(this, _capacity); }

    class const_iterator
        : public std::iterator<std::forward_iterator_tag,
                               const std::pair<const KeyType, ValueType>> {
     public:
        const_iterator(
            const HashMap<KeyType, ValueType, Hash>* _owner = nullptr,
            const size_t index = 0)
            : index(index), owner(_owner) {}
        const std::pair<const KeyType, ValueType>& operator*() const {
            return owner->items[index]->el;
        }
        const std::pair<const KeyType, ValueType>* operator->() const {
            return &**this;
        }

        const const_iterator& operator++() {
            ++index;
            for (int i = index; i < owner->_capacity; ++i) {
                if (owner->items[i] &&
                    owner->items[i]->state == item_state::USED) {
                    break;
                }
                ++index;
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++index;
            for (int i = index; i < owner->_capacity; ++i) {
                if (owner->items[i] &&
                    owner->items[i]->state == item_state::USED) {
                    break;
                }
                ++index;
            }
            return temp;
        }

        bool operator==(const const_iterator& other) const {
            return owner == other.owner && index == other.index;
        }
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

     private:
        size_t index = 0;
        const HashMap<KeyType, ValueType, Hash>* owner;
    };

    const_iterator begin() const {
        int index = 0;
        for (int i = 0; i < _capacity; ++i) {
            if (items[i] && items[i]->state == item_state::USED) {
                break;
            }
            ++index;
        }
        return const_iterator(this, index);
    }
    const_iterator end() const { return const_iterator(this, _capacity); }

    iterator find(const KeyType& key) {
        size_t index = get_index(key, _capacity);
        for (int i = 0; i < _capacity; ++i) {
            if (items[index] == 0) {
                return end();
            }
            if (items[index]->el.first == key) {
                if (items[index]->state == item_state::USED) {
                    return iterator(this, index);
                } else {
                    return end();
                }
            }
            ++index;
            if (index == _capacity) {
                index = 0;
            }
        }
        return end();
    }

    const_iterator find(KeyType key) const {
        size_t index = get_index(key, _capacity);
        for (int i = 0; i < _capacity; ++i) {
            if (items[index] == 0) {
                return end();
            }
            if (items[index]->el.first == key) {
                if (items[index]->state == item_state::USED) {
                    return const_iterator(this, index);
                } else {
                    return end();
                }
            }
            ++index;
            if (index == _capacity) {
                index = 0;
            }
        }
        return end();
    }

    ValueType& operator[](const KeyType& key) {
        auto it = find(key);
        if (it == end()) {
            add(std::make_pair(key, ValueType()), items, _capacity);
            check_rehash();
        }

        return find(key)->second;
    }

    const ValueType& at(KeyType key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("gg");
        } else {
            return it->second;
        }
    }

    void clear() {
        if (empty()) {
            return;
        }
        _size = 0;
        for (int i = 0; i < _capacity; ++i) {
            items[i] = nullptr;
        }
        _capacity = kDefaultCapacity;
        items.assign(_capacity, nullptr);
    }

 private:
    static const size_t kDefaultCapacity = 5;

    size_t _size = 0;
    size_t _capacity;
    Hash _hash;

    enum class item_state { FREE, USED, ERASED };

    struct Item {
        std::pair<const KeyType, ValueType> el;
        item_state state;
        Item(std::pair<const KeyType, ValueType> item,
             item_state _state = item_state::FREE)
            : el(item), state(_state) {}
    };

    std::vector<std::shared_ptr<Item>> items;

    friend class iterator;
    friend class const_iterator;

    size_t get_index(KeyType key, size_t capacity) const {
        return _hash(key) % capacity;
    }

    void rehash(size_t new_capacity) {
        new_capacity =
            std::max(new_capacity, static_cast<size_t>(kDefaultCapacity));
        std::vector<std::shared_ptr<Item>> new_items(new_capacity);

        for (size_t i = 0; i < _capacity; ++i) {
            if (items[i] && items[i]->state == item_state::USED) {
                add(items[i]->el, new_items, new_capacity);
            }
        }
        _size /= 2;
        items = new_items;
        _capacity = new_capacity;
    }

    void add(std::pair<const KeyType, ValueType> item,
             std::vector<std::shared_ptr<Item>>& _items, size_t capacity) {
        size_t index = get_index(item.first, capacity);
        size_t res_index = capacity;
        for (int i = 0; i < capacity; ++i) {
            if (_items[index] == 0) {
                if (res_index != capacity) {
                    _items[res_index] =
                        std::make_shared<Item>(item, item_state::USED);
                } else {
                    _items[index] =
                        std::make_shared<Item>(item, item_state::USED);
                }
                ++_size;
                return;
            } else {
                if (_items[index] &&
                    _items[index]->state == item_state::ERASED &&
                    res_index == capacity) {
                    res_index = index;
                } else {
                    if (_items[index] &&
                        _items[index]->state == item_state::USED &&
                        _items[index]->el.first == item.first) {
                        return;
                    }
                }
            }
            ++index;
            if (index == capacity) {
                index = 0;
            }
        }
    }

    void check_rehash() {
        if (_size * 2 > _capacity) {
            rehash(_capacity * 2);
        }
    }

    void check_rehash_erase() {
        if (_size << 2 < _capacity) {
            rehash(_capacity - _capacity / 3);
        }
    }
};
