#ifndef SINGLETON_H
#define SINGLETON_H

template<typename T>
class Singleton {
public:
    template<typename U>
    static T* by() {
        static T instance;
        return &instance;
    }
};

#endif // SINGLETON_H 