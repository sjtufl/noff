//
// Created by root on 18-4-16.
//

#ifndef NOFF_NONCOPYABLE_H
#define NOFF_NONCOPYABLE_H

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

#endif //NOFF_NONCOPYABLE_H
