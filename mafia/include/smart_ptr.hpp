#pragma once

#include <utility>
#include <cstddef>


template<class T>
class smart_ptr {
    T *ptr{};           // управляемый объект
    unsigned *cnt{};    // разделяемый счётчик ссылок

    void release()

    noexcept {
        if (!cnt) return;
        if (--(*cnt) == 0) {
            delete ptr;
            delete cnt;
        }
        ptr = nullptr;
        cnt = nullptr;
    }

public:
    // ctor по умолчанию: "пустой" указатель с собственным счётчиком (=1)
    smart_ptr() : ptr(nullptr), cnt(new unsigned(1)) {}

    // явный владелец сырым указателем
    explicit smart_ptr(T *p) : ptr(p), cnt(new unsigned(1)) {}

    // копирование: делим владение, ++refcount
    smart_ptr(const smart_ptr &o) : ptr(o.ptr), cnt(o.cnt) { ++(*cnt); }

    // перемещение: забираем владение, источник обнуляем
    smart_ptr(smart_ptr &&o)

    noexcept
            : ptr(std::exchange(o.ptr, nullptr)), cnt(std::exchange(o
    .cnt, nullptr)) {}

    // копирующее присваивание
    smart_ptr &operator=(const smart_ptr &o) {
        if (this != &o) {
            release();
            ptr = o.ptr;
            cnt = o.cnt;
            ++(*cnt);
        }
        return *this;
    }

    // перемещающее присваивание
    smart_ptr &operator=(smart_ptr &&o)

    noexcept {
        if (this != &o) {
            release();
            ptr = std::exchange(o.ptr, nullptr);
            cnt = std::exchange(o.cnt, nullptr);
        }
        return *this;
    }

    ~smart_ptr() { release(); }

    // разыменование (оба вида)
    T &operator*() const { return *ptr; }

    T *operator->() const { return ptr; }

    // доступ к сырому указателю (без изменения владения)
    T *get() const { return ptr; }

    // сброс владения и установка нового объекта (или nullptr)
    void reset(T *p = nullptr) {
        release();
        ptr = p;
        cnt = new unsigned(1);
    }

    // обмен внутренними указателями/счётчиком
    void swap(smart_ptr &o)

    noexcept {
        std::swap(ptr, o.ptr);
        std::swap(cnt, o.cnt);
    }

    // сколько владельцев сейчас указывает на объект
    unsigned use_count() const { return cnt ? *cnt : 0U; }

    // сравнение по адресу управляемого объекта
    bool operator==(const smart_ptr &o) const { return ptr == o.ptr; }

    bool operator!=(const smart_ptr &o) const { return ptr != o.ptr; }
};

// Утилита наподобие make_shared: удобное создание smart_ptr<T>
template<class T, class... Args>
smart_ptr<T> make_smart(Args &&... args) {
    return smart_ptr<T>(new T(std::forward<Args>(args)...));
}
