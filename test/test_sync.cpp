
#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>

#include "test.h"

#include "libipc/platform/detail.h"
#if defined(IPC_OS_LINUX_)
#include <pthread.h>
#include <time.h>

TEST(PThread, Robust) {
    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&ma, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&mutex, &ma);

    std::thread{[&mutex] {
        pthread_mutex_lock(&mutex);
        // pthread_mutex_unlock(&mutex);
    }}.join();

    struct timespec tout;
    clock_gettime(CLOCK_REALTIME, &tout);
    int r = pthread_mutex_timedlock(&mutex, &tout);
    EXPECT_EQ(r, EOWNERDEAD);

    pthread_mutex_consistent(&mutex);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
}
#elif defined(IPC_OS_WINDOWS_)
#include <Windows.h>
#include <tchar.h>

TEST(PThread, Robust) {
    HANDLE lock = CreateMutex(NULL, FALSE, _T("test-robust"));
    std::thread{[] {
        HANDLE lock = CreateMutex(NULL, FALSE, _T("test-robust"));
        WaitForSingleObject(lock, 0);
    }}.join();

    DWORD r = WaitForSingleObject(lock, 0);
    EXPECT_EQ(r, WAIT_ABANDONED);

    CloseHandle(lock);
}
#endif // OS

#include "libipc/mutex.h"

TEST(Sync, Mutex) {
    ipc::sync::mutex lock;
    EXPECT_TRUE(lock.open("test-mutex-robust"));
    std::thread{[] {
        ipc::sync::mutex lock{"test-mutex-robust"};
        EXPECT_TRUE(lock.valid());
        EXPECT_TRUE(lock.lock());
    }}.join();

    EXPECT_THROW(lock.try_lock(), std::system_error);

    int i = 0;
    EXPECT_TRUE(lock.lock());
    i = 100;
    auto t2 = std::thread{[&i] {
        ipc::sync::mutex lock{"test-mutex-robust"};
        EXPECT_TRUE(lock.valid());
        EXPECT_FALSE(lock.try_lock());
        EXPECT_TRUE(lock.lock());
        i += i;
        EXPECT_TRUE(lock.unlock());
    }};
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(i, 100);
    EXPECT_TRUE(lock.unlock());
    t2.join();
    EXPECT_EQ(i, 200);
}

#include "libipc/semaphore.h"

TEST(Sync, Semaphore) {
    ipc::sync::semaphore sem;
    EXPECT_TRUE(sem.open("test-sem"));
    std::thread{[] {
        ipc::sync::semaphore sem{"test-sem"};
        EXPECT_TRUE(sem.post(10));
    }}.join();

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(sem.wait(0));
    }
    EXPECT_FALSE(sem.wait(0));
}