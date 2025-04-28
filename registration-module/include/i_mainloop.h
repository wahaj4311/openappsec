#ifndef I_MAINLOOP_H
#define I_MAINLOOP_H

#include <functional>
#include <chrono>

class I_MainLoop {
public:
    enum class RoutineType { RealTime, System, Timer, Offline };
    
    virtual ~I_MainLoop() = default;
    virtual void addFileRoutine(
        RoutineType type,
        int fd,
        std::function<void()> routine,
        const std::string& name,
        bool is_primary = false
    ) = 0;
    
    virtual void addRecurringRoutine(
        RoutineType type,
        std::chrono::seconds interval,
        std::function<void()> routine,
        const std::string& name,
        bool is_primary = false
    ) = 0;
    
    virtual void addOneTimeRoutine(
        RoutineType type,
        std::function<void()> routine,
        const std::string& name
    ) = 0;
};

#endif // I_MAINLOOP_H 